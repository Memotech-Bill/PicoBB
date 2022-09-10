// net_cyw43.c - Network support for Pico W

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "BBC.h"
#include "network.h"
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>
#include <lwip/dns.h>

#define STATE_COMPLETED     0
#define STATE_WAITING       1

static uint32_t net_interrupts;
static err_t last_error = 0;

static const char *psError[] =
    {
    "No error.",                    //   0 - ERR_OK
    "Out of memory error.",         //  -1 - ERR_MEM
    "Buffer error.",                //  -2 - ERR_BUF
    "Timeout.",                     //  -3 - ERR_TIMEOUT
    "Routing problem.",             //  -4 - ERR_RTE
    "Operation in progress",        //  -5 - ERR_INPROGRESS
    "Illegal value.",               //  -6 - ERR_VAL
    "Operation would block.",       //  -7 - ERR_WOULDBLOCK
    "Address in use.",              //  -8 - ERR_USE
    "Already connecting.",          //  -9 - ERR_ALREADY
    "Conn already established.",    // -10 - ERR_ISCONN
    "Not connected.",               // -11 - ERR_CONN
    "Low-level netif error",        // -12 - ERR_IF
    "Connection aborted.",          // -13 - ERR_ABRT
    "Connection reset.",            // -14 - ERR_RST
    "Connection closed.",           // -15 - ERR_CLSD
    "Illegal argument. ",           // -16 - ERR_ARG
    };

static inline int net_error (err_t err)
    {
    if ( err != 0 ) last_error = err;
    return err;
    }

int net_last_errno (void)
    {
    return last_error;
    }

const char *net_error_desc (int err)
    {
    if (( err <= 0 ) && ( err > -( sizeof (psError) / sizeof (psError[0]) )))
        {
        return psError[-last_error];
        }
    return "Unknown error.";
    }

void net_error_clear (void)
    {
    last_error = 0;
    }

static inline void net_crit_begin (void)
    {
    net_interrupts = save_and_disable_interrupts ();
    }

static inline void net_crit_end (void)
    {
    restore_interrupts (net_interrupts);
    }

static inline void net_wait (void)
    {
#if HAVE_CYW43 == 2
    sleep_ms (1);
    cyw43_arch_poll ();
#else
    __wfi ();
#endif
    }

static absolute_time_t net_tend;
static inline bool net_continue (void)
    {
    return ( absolute_time_diff_us (net_tend, get_absolute_time ()) < 0 );
    }

typedef struct s_scan_cb_result
    {
    struct s_scan_cb_result     *next;
    net_scan_result_t           scan;
    } scan_cb_result_t;

static volatile scan_cb_result_t    *scan_first = NULL;
static volatile scan_cb_result_t    *scan_last = NULL;
static int scan_err = ERR_OK;

static int net_scan_cb (void *env, const cyw43_ev_scan_result_t *result)
    {
    if ( ( scan_err == ERR_OK ) && result )
        {
        scan_cb_result_t *psr = (scan_cb_result_t *) malloc (sizeof (scan_cb_result_t));
        if ( psr != NULL )
            {
            psr->next = NULL;
            strncpy (psr->scan.ssid, result->ssid, result->ssid_len);
            psr->scan.ssid[result->ssid_len] = '\0';
            memcpy (psr->scan.mac, result->bssid, 6);
            psr->scan.chan = result->channel;
            psr->scan.rssi = result->rssi;
            psr->scan.auth = result->auth_mode;
            if ( scan_last == NULL )
                {
                scan_first = psr;
                scan_last = psr;
                }
            else
                {
                scan_last->next = psr;
                scan_last = psr;
                }
            }
        else
            {
            scan_err = ERR_MEM;
            }
        }
    return 0;
    }

int net_wifi_scan (net_scan_result_t *pscan)
    {
    static bool bScan = false;
    if ( ! bScan )
        {
        while ( cyw43_wifi_scan_active (&cyw43_state) )
            {
            net_wait ();
            }
        scan_first = NULL;
        scan_last = NULL;
        cyw43_wifi_scan_options_t scan_options = {0};
        int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, net_scan_cb);
        if ( err != 0 ) return net_error (err);
        scan_err = ERR_OK;
        bScan = true;
        }
    while ( ( scan_first == NULL ) && ( scan_err == ERR_OK )
        && ( cyw43_wifi_scan_active (&cyw43_state) ) )
        {
        net_wait ();
        }
    if ( scan_first != NULL )
        {
        scan_cb_result_t *psr = (scan_cb_result_t *) scan_first;
        net_crit_begin ();
        scan_first = psr->next;
        if ( scan_first == NULL ) scan_last = NULL;
        net_crit_end ();
        memcpy (pscan, &psr->scan, sizeof (net_scan_result_t));
        free (psr);
        return ERR_OK;
        }
    memset (pscan, 0, sizeof (net_scan_result_t));
    bScan = false;
    if ( scan_err == ERR_OK ) scan_err = ERR_CLSD;
    return net_error (scan_err);
    }

const cyw43_t *net_wifi_get_state (void)
    {
    return &cyw43_state;
    }

const struct netif *net_wifi_get_netif (int iface)
    {
    if ((iface >= 0) && (iface <= 1)) return &cyw43_state.netif[iface];
    return NULL;
    }

int net_wifi_get_ipaddr (int iface, ip_addr_t *ipaddr)
    {
    if ((iface >= 0) && (iface <= 1))
        {
        if ( cyw43_tcpip_link_status (&cyw43_state, iface) == CYW43_LINK_UP )
            {
            memcpy (ipaddr, &cyw43_state.netif[iface].ip_addr, sizeof (ip_addr_t));
            return ERR_OK;
            }
        return net_error (ERR_CONN);
        }
    return net_error (ERR_ARG);
    }

typedef struct s_tcp_conn
    {
    struct tcp_pcb      *pcb;
    struct pbuf         *p;
    struct tcp_pcb      *acc;
    int                 nused;
    int                 ndata;
    volatile int        err;
    struct s_tcp_conn   *next;
    } tcp_conn_t;

static tcp_conn_t  *tconn_active = NULL;
static tcp_conn_t  *tconn_free = NULL;

static tcp_conn_t *tcp_conn_alloc (void)
    {
    tcp_conn_t *conn = NULL;
    if ( tconn_free != NULL )
        {
        conn = tconn_free;
        tconn_free = conn->next;
        memset (conn, 0, sizeof (tcp_conn_t));
        }
    else
        {
        conn = (tcp_conn_t *) calloc (1, sizeof (tcp_conn_t));
        }
    if ( conn != NULL )
        {
        conn->next = tconn_active;
        tconn_active = conn;
        }
    return conn;
    }

static void tcp_conn_free (tcp_conn_t *conn)
    {
    if ( conn == tconn_active )
        {
        tconn_active = conn->next;
        }
    else
        {
        tcp_conn_t *pconn = tconn_active;
        while (pconn != NULL)
            {
            if ( conn == pconn->next )
                {
                pconn->next = conn->next;
                break;
                }
            pconn = pconn->next;
            }
        }
    conn->next = tconn_free;
    tconn_free = conn;
    }

int net_tcp_valid (intptr_t connin)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    tcp_conn_t *tconn = tconn_active;
    while ( tconn != NULL )
        {
        if ( conn == tconn ) return 1;
        tconn = tconn->next;
        }
    return 0;
    }

static void net_tcp_err_cb (void *connin, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->err = err;
    }

static err_t net_tcp_connected_cb (void *connin, struct tcp_pcb *pcb, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->err = err;
    return ERR_OK;
    }

static err_t net_tcp_receive_cb (void *connin, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( pcb == conn->acc )
        {
        if ( p == NULL )
            {
            conn->acc = NULL;
            return ERR_OK;
            }
        return net_error (ERR_BUF);
        }
    if ( p == NULL )
        {
        conn->err = ERR_RST;
        return ERR_OK;
        }
    if ( conn->p == NULL )
        {
        conn->p = p;
        conn->nused = 0;
        }
    else
        {
        pbuf_cat (conn->p, p);
        }
    return ERR_OK;
    }

intptr_t net_tcp_connect (const ip_addr_t *ipaddr, uint32_t port, uint32_t timeout)
    {
    tcp_conn_t *conn = tcp_conn_alloc ();
    if ( conn == NULL ) return net_error (ERR_MEM);
    conn->pcb = tcp_new ();
    if ( conn->pcb == NULL )
        {
        tcp_conn_free (conn);
        return net_error (ERR_MEM);
        }
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    conn->err = STATE_WAITING;
    tcp_arg (conn->pcb, conn);
    tcp_err (conn->pcb, net_tcp_err_cb);
    cyw43_arch_lwip_begin();
    tcp_connect (conn->pcb, ipaddr, port, net_tcp_connected_cb);
    cyw43_arch_lwip_end();
    while (( conn->err == STATE_WAITING ) && net_continue () )
        {
        net_wait ();
        }
    if ( conn->err != STATE_COMPLETED )
        {
        err_t err = conn->err;
        cyw43_arch_lwip_begin();
        tcp_abort (conn->pcb);
        cyw43_arch_lwip_end();
        tcp_conn_free (conn);
        if ( conn->err == STATE_WAITING ) return net_error (ERR_TIMEOUT);
        return err;
        }
    tcp_recv (conn->pcb, net_tcp_receive_cb);
    return  (intptr_t) conn;
    }

static err_t net_tcp_accept_cb (void *connin, struct tcp_pcb *pcb, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->acc == NULL )
        {
        conn->acc = pcb;
        tcp_arg (pcb, conn);
        tcp_recv (pcb, net_tcp_receive_cb);
        return ERR_OK;
        }
    else
        {
        tcp_abort (pcb);
        return ERR_ABRT;
        }
    }

intptr_t net_tcp_listen (const ip_addr_t *ipaddr, uint32_t port)
    {
    tcp_conn_t *conn = tcp_conn_alloc ();
    if ( conn == NULL ) return net_error (ERR_MEM);
    conn->pcb = tcp_new ();
    if ( conn->pcb == NULL )
        {
        tcp_conn_free (conn);
        return net_error (ERR_MEM);
        }
    conn->err = STATE_WAITING;
    tcp_arg (conn->pcb, conn);
    tcp_err (conn->pcb, net_tcp_err_cb);
    cyw43_arch_lwip_begin();
    err_t err = tcp_bind (conn->pcb, ipaddr, port);
    cyw43_arch_lwip_end();
    if ( err == ERR_OK )
        {
        cyw43_arch_lwip_begin();
        struct tcp_pcb *pcb = tcp_listen (conn->pcb);
        cyw43_arch_lwip_end();
        if ( pcb == NULL )  err = ERR_MEM;
        else                conn->pcb = pcb;
        }
    if ( err != ERR_OK )
        {
        cyw43_arch_lwip_begin();
        tcp_abort (conn->pcb);
        cyw43_arch_lwip_end();
        tcp_conn_free (conn);
        return net_error (err);
        }
    tcp_accept (conn->pcb, net_tcp_accept_cb);
    return (intptr_t) conn;
    }

intptr_t net_tcp_accept (intptr_t listen)
    {
    tcp_conn_t *conn = (tcp_conn_t *) listen;
    if ( conn->acc == NULL ) return 0;
    struct tcp_pcb *pcb = conn->acc;
    conn->acc = NULL;
    err_t err = ERR_OK;
    tcp_conn_t *nconn = tcp_conn_alloc ();
    if ( nconn == NULL )
        {
        cyw43_arch_lwip_begin();
        tcp_abort (pcb);
        cyw43_arch_lwip_end();
        return net_error (ERR_MEM);
        }
    nconn->pcb = pcb;
    tcp_arg (nconn->pcb, nconn);
    tcp_err (nconn->pcb, net_tcp_err_cb);
    tcp_recv (nconn->pcb, net_tcp_receive_cb);
    return (intptr_t) nconn;
    }

static err_t net_tcp_sent_cb (void *connin, struct tcp_pcb *pcb, uint16_t len)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->ndata -= len;
    if ( conn->ndata <= 0 ) conn->err = STATE_COMPLETED;
    return ERR_OK;
    }

int net_tcp_write (intptr_t connin, uint32_t len, void *data, uint32_t timeout)
    {
    if ( ! net_tcp_valid (connin) ) return net_error (ERR_ARG);
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->err != ERR_OK ) return net_error (conn->err);
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    int nsent = 0;
    while ( len > 0 )
        {
        int nsend = tcp_sndbuf (conn->pcb);
        if ( nsend > 0 )
            {
            uint8_t flags = TCP_WRITE_FLAG_MORE;
            if ( nsend >= len )
                {
                flags = 0;
                nsend = len;
                }
            conn->ndata = nsend;
            conn->err = STATE_WAITING;
            tcp_sent (conn->pcb, net_tcp_sent_cb);
            cyw43_arch_lwip_begin();
            tcp_write (conn->pcb, data, nsend, flags);
            cyw43_arch_lwip_end();
            while (( conn->err == STATE_WAITING ) && net_continue () )
                {
                net_wait ();
                }
            if ( conn->err != STATE_COMPLETED )
                {
                cyw43_arch_lwip_begin();
                tcp_abort (conn->pcb);
                cyw43_arch_lwip_end();
                return net_error (conn->err);
                }
            data += nsend;
            len -= nsend;
            nsent += nsend;
            }
        else
            {
            net_wait ();
            }
        }
    return nsent;
    }

int net_tcp_read (intptr_t connin, uint32_t len, void *data)
    {
    if ( ! net_tcp_valid (connin) ) return net_error (ERR_ARG);
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->err != ERR_OK ) return net_error (conn->err);
    int nrecv = 0;
    while (( len > 0 ) && ( conn->p != NULL ))
        {
        struct pbuf *p = conn->p;
        int ndata = p->len - conn->nused;
        if ( ndata > len ) ndata = len;
        memcpy (data, (uint8_t *)p->payload + conn->nused, ndata);
        conn->nused += ndata;
        nrecv += ndata;
        data += ndata;
        len -= ndata;
        if ( conn->nused == p->len )
            {
            cyw43_arch_lwip_begin();
            tcp_recved (conn->pcb, p->len);
            conn->p = p->next;
            conn->nused = 0;
            pbuf_ref (p->next);
            pbuf_free (p);
            cyw43_arch_lwip_end();
            }
        }
    return nrecv;
    }

void net_tcp_close (intptr_t connin)
    {
    if ( ! net_tcp_valid (connin) ) return;
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->p != NULL )
        {
        cyw43_arch_lwip_begin();
        pbuf_free (conn->p);
        cyw43_arch_lwip_end();
        }
    if ( conn->acc != NULL )
        {
        cyw43_arch_lwip_begin();
        tcp_abort (conn->acc);
        cyw43_arch_lwip_end();
        }
    while ( true )
        {
        cyw43_arch_lwip_begin();
        err_t err = tcp_close (conn->pcb);
        cyw43_arch_lwip_end();
        if ( err == ERR_OK ) break;
        net_wait ();
        }
    tcp_conn_free (conn);
    }

void net_tcp_peer (intptr_t connin, ip_addr_t *ipaddr, uint32_t *port)
    {
    if ( ! net_tcp_valid (connin) )
        {
        memset (ipaddr, 0, sizeof (ip_addr_t));
        ISTORE (port, 0);
        return;
        }
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    ip_addr_t raddr;
    uint16_t rport;
    cyw43_arch_lwip_begin();
    tcp_tcp_get_tcp_addrinfo (conn->pcb, 0, &raddr, &rport);
    cyw43_arch_lwip_end();
    memcpy (ipaddr, &raddr, sizeof (ip_addr_t));
    ISTORE (port, rport);
    }

typedef struct s_find_ip
    {
    bool        found;
    ip_addr_t   ipaddr;
    } find_ip_t;

void net_dns_found_cb (const char *host, const ip_addr_t *addr, void *pfipin)
    {
    find_ip_t *pfip = (find_ip_t *) pfipin;
    pfip->ipaddr = *addr;
    pfip->found = true;
    }

int net_dns_get_ip (const char *host, uint32_t timeout, ip_addr_t *ipaddr)
    {
    find_ip_t   fip = { false, 0 };
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    err_t err = dns_gethostbyname (host, &fip.ipaddr, net_dns_found_cb, &fip);
    if ( err != ERR_OK ) return net_error (err);
    while (( ! fip.found ) && ( net_continue () ))
        {
        net_wait ();
        }
    if ( fip.found )
        {
        memcpy (ipaddr, &fip.ipaddr, sizeof (ip_addr_t));
        return ERR_OK;
        }
    return net_error (ERR_TIMEOUT);
    }

typedef struct s_udp_conn
    {
    struct udp_pcb      *pcb;
    struct pbuf         *p;
    ip_addr_t           addr;
    uint16_t            port;
    int                 nused;
    struct s_udp_conn   *next;
    } udp_conn_t;

static udp_conn_t  *uconn_active = NULL;
static udp_conn_t  *uconn_free = NULL;

static udp_conn_t *udp_conn_alloc (void)
    {
    udp_conn_t *conn = NULL;
    if ( uconn_free != NULL )
        {
        conn = uconn_free;
        uconn_free = conn->next;
        memset (conn, 0, sizeof (udp_conn_t));
        }
    else
        {
        conn = (udp_conn_t *) calloc (1, sizeof (udp_conn_t));
        }
    if ( conn != NULL )
        {
        conn->next = uconn_active;
        uconn_active = conn;
        }
    return conn;
    }

static void udp_conn_free (udp_conn_t *conn)
    {
    if ( conn == uconn_active )
        {
        uconn_active = conn->next;
        }
    else
        {
        udp_conn_t *pconn = uconn_active;
        while (pconn != NULL)
            {
            if ( conn == pconn->next )
                {
                pconn->next = conn->next;
                break;
                }
            pconn = pconn->next;
            }
        }
    conn->next = uconn_free;
    uconn_free = conn;
    }

int net_udp_valid (intptr_t connin)
    {
    udp_conn_t *conn = (udp_conn_t *) connin;
    udp_conn_t *uconn = uconn_active;
    while ( uconn != NULL )
        {
        if ( conn == uconn ) return 1;
        uconn = uconn->next;
        }
    return 0;
    }

static void net_udp_receive_cb (void *connin, struct udp_pcb *pcb, struct pbuf *p,
    const ip_addr_t *addr, uint16_t port)
    {
    udp_conn_t *conn = (udp_conn_t *) connin;
    if ( conn->p == NULL )
        {
        conn->p = p;
        conn->addr = *addr;
        conn->port = port;
        }
    else if (( memcmp (&conn->addr, addr, sizeof (ip_addr_t)) == 0 ) && ( conn->port == port ))
        {
        pbuf_cat (conn->p, p);
        }
    else
        {
        pbuf_free (p);
        }
    }

intptr_t net_udp_open (void)
    {
    udp_conn_t *conn = udp_conn_alloc ();
    if ( conn == NULL ) return net_error (ERR_MEM);
    conn->pcb = udp_new ();
    if ( conn->pcb == NULL )
        {
        udp_conn_free (conn);
        return net_error (ERR_MEM);
        }
    udp_recv (conn->pcb, net_udp_receive_cb, conn);
    return (intptr_t) conn;
    }

int net_udp_bind (intptr_t connin, const ip_addr_t *ipaddr, uint32_t port)
    {
    if ( ! net_udp_valid (connin) ) return net_error (ERR_ARG);
    udp_conn_t *conn = (udp_conn_t *) connin;
    cyw43_arch_lwip_begin();
    err_t err = udp_bind (conn->pcb, ipaddr, port);
    cyw43_arch_lwip_end();
    return net_error (err);
    }

int net_udp_send (intptr_t connin, uint32_t len, void *data, ip_addr_t *ipaddr, uint32_t port)
    {
    if ( ! net_udp_valid (connin) ) return net_error (ERR_ARG);
    udp_conn_t *conn = (udp_conn_t *) connin;
    struct pbuf *p = pbuf_alloc (PBUF_TRANSPORT, len, PBUF_RAM);
    if ( p == NULL ) return net_error (ERR_MEM);
    memcpy (p->payload, data, len);
    cyw43_arch_lwip_begin();
    err_t err = udp_sendto (conn->pcb, p, ipaddr, port);
    cyw43_arch_lwip_end();
    pbuf_free (p);
    return net_error (err);
    }

int net_udp_recv (intptr_t connin, uint32_t len, void *data, ip_addr_t *ipaddr, uint32_t *port)
    {
    if ( ! net_udp_valid (connin) ) return net_error (ERR_ARG);
    udp_conn_t *conn = (udp_conn_t *) connin;
    int nrecv = 0;
    while (( len > 0 ) && ( conn->p != NULL ))
        {
        struct pbuf *p = conn->p;
        int ndata = p->len - conn->nused;
        if ( ndata > len ) ndata = len;
        memcpy (data, (uint8_t *)p->payload + conn->nused, ndata);
        conn->nused += ndata;
        nrecv += ndata;
        data += ndata;
        len -= ndata;
        if ( conn->nused == p->len )
            {
            cyw43_arch_lwip_begin();
            conn->p = p->next;
            conn->nused = 0;
            pbuf_ref (p->next);
            pbuf_free (p);
            cyw43_arch_lwip_end();
            }
        }
    memcpy (ipaddr, &conn->addr, sizeof (ip_addr_t));
    ISTORE (port, conn->port);
    return nrecv;
    }

void net_udp_close (intptr_t connin)
    {
    if ( ! net_udp_valid (connin) ) return;
    udp_conn_t *conn = (udp_conn_t *) connin;
    cyw43_arch_lwip_begin();
    udp_disconnect (conn->pcb);
    udp_remove (conn->pcb);
    cyw43_arch_lwip_end();
    udp_conn_free (conn);
    }

void net_freeall (void)
    {
    while ( tconn_active != NULL )
        {
        net_tcp_close ((intptr_t) tconn_active);
        }
    while ( tconn_free != NULL )
        {
        tcp_conn_t *conn = tconn_free;
        tconn_free = conn->next;
        free ((void *) conn);
        }
    while ( uconn_active != NULL )
        {
        net_udp_close ((intptr_t) uconn_active);
        }
    while ( uconn_free != NULL )
        {
        udp_conn_t *conn = uconn_free;
        uconn_free = conn->next;
        free ((void *) conn);
        }
    last_error = 0;
    }
