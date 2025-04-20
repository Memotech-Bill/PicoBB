// net_cyw43.c - Network support for Pico W

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <hardware/sync.h>
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>
#include <lwip/dns.h>
#include "BBC.h"
#include "network.h"

#if CYW43_LWIP != 1
#error  CYW43 Support not enabled
#endif

#define NUM_SCAN_RESULT     5
#define NUM_UDP_MSG         5
#define MALLOC_OVERHEAD     8

#ifndef DIAG
#define DIAG    0
#endif
#if DIAG
#define DPRINT  printf
#else
#define DPRINT(...)
#endif

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

#if NET_HEAP

// Allocate storage on the BASIC heap
#include "heap.h"
void memp_size (int *count, int *size);

#else

// Use C heap
#define heap_malloc  malloc
#define heap_calloc  calloc
#define heap_free    free

#endif

static inline int net_error (err_t err)
    {
    if ( err != 0 )
        {
        DPRINT ("net_error (%d): %s\n", err, (( err <= 0 ) && ( err > -( sizeof (psError) / sizeof (psError[0]) ))) ? psError[-err] : "Unknown");
        last_error = err;
        }
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
    if ( absolute_time_diff_us (net_tend, get_absolute_time ()) < 0 ) return true;
    DPRINT ("Timeout: net_tend = %ld\n", net_tend);
    return false;
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
        scan_cb_result_t *psr = (scan_cb_result_t *) heap_malloc (sizeof (scan_cb_result_t));
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
        heap_free (psr);
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
        conn = (tcp_conn_t *) heap_calloc (1, sizeof (tcp_conn_t));
        }
    if ( conn != NULL )
        {
        conn->next = tconn_active;
        tconn_active = conn;
        }
    DPRINT ("tcp_conn_alloc () = %p\n", conn);
    return conn;
    }

static void tcp_conn_free (tcp_conn_t *conn)
    {
    DPRINT ("tcp_conn_free (%p)\n", conn);
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
    DPRINT ("net_tcp_err_cb (%p, %d)\n", connin, err);
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->err = err;
    }

static err_t net_tcp_connected_cb (void *connin, struct tcp_pcb *pcb, err_t err)
    {
    DPRINT ("net_tcp_err_cb (%p, %p, %d)\n", connin, pcb, err);
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->err = err;
    return ERR_OK;
    }

static err_t net_tcp_receive_cb (void *connin, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
    {
    DPRINT ("net_tcp_receive_cb (%p, %p, %d)\n", connin, pcb, err);
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
    DPRINT ("net_tcp_connect (%s (0x%08X), %d, %d)\n", ipaddr_ntoa (ipaddr), *ipaddr, port, timeout);
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
    DPRINT ("conn->err = %d\n", conn->err);
    if ( conn->err != STATE_COMPLETED )
        {
        DPRINT ("Abort: conn = %p\n", conn);
        err_t err = conn->err;
        cyw43_arch_lwip_begin();
        tcp_abort (conn->pcb);
        cyw43_arch_lwip_end();
        tcp_conn_free (conn);
        DPRINT ("conn->err = %d\n", conn->err);
        if ( err == STATE_WAITING ) return net_error (ERR_TIMEOUT);
        return net_error (err);
        }
    tcp_recv (conn->pcb, net_tcp_receive_cb);
    DPRINT ("Connected: conn = %p\n", conn);
    return  (intptr_t) conn;
    }

static err_t net_tcp_accept_cb (void *connin, struct tcp_pcb *pcb, err_t err)
    {
    DPRINT ("net_tcp_accept_cb (%p, %p, %d)\n", connin, pcb, err);
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->acc == NULL )
        {
        DPRINT ("Accept connection\n");
        conn->acc = pcb;
        tcp_arg (pcb, conn);
        tcp_recv (pcb, net_tcp_receive_cb);
        return ERR_OK;
        }
    DPRINT ("Reject connection\n");
    tcp_abort (pcb);
    return ERR_ABRT;
    }

intptr_t net_tcp_listen (const ip_addr_t *ipaddr, uint32_t port)
    {
    DPRINT ("net_tcp_listen (%p, %d)\n", ipaddr, port);
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
    DPRINT ("net_tcp_accept (%p)\n", listen);
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
    DPRINT ("net_tcp_sent_cb (%p, %p, %d)\n", connin, pcb, len);
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->ndata -= len;
    if ( conn->ndata <= 0 ) conn->err = STATE_COMPLETED;
    return ERR_OK;
    }

int net_tcp_write (intptr_t connin, uint32_t len, void *data, uint32_t timeout)
    {
    DPRINT ("net_tcp_write (%p, %d, %p, %d)\n", connin, len, data, timeout);
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
                DPRINT ("Abort: conn = %p\n", conn);
                err_t err = conn->err;
                cyw43_arch_lwip_begin();
                tcp_abort (conn->pcb);
                cyw43_arch_lwip_end();
                DPRINT ("conn->err = %d\n", conn->err);
                if ( err == STATE_WAITING ) return net_error (ERR_TIMEOUT);
                return net_error (err);
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
#if DIAG
    if ( conn->p ) DPRINT ("net_tcp_read (%p, %d, %p)\n", connin, len, data);
#endif
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
    DPRINT ("net_tcp_close (%p)\n", connin);
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
    DPRINT ("net_tcp_peer (%p, %p, %p)\n", connin, ipaddr, port);
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
    int         err;
    ip_addr_t   ipaddr;
    } find_ip_t;

void net_dns_found_cb (const char *host, const ip_addr_t *addr, void *pfipin)
    {
    DPRINT ("net_dns_found_cb (%s, %p, %p)\n", host, addr, pfipin);
    find_ip_t *pfip = (find_ip_t *) pfipin;
    if (addr != NULL)
        {
        DPRINT ("ipaddr = %s (0x%08X)\n", ipaddr_ntoa (addr), *addr);
        pfip->ipaddr = *addr;
        pfip->err = ERR_OK;
        }
    else
        {
        pfip->err = ERR_CLSD;
        }
    }

int net_dns_get_ip (const char *host, uint32_t timeout, ip_addr_t *ipaddr)
    {
    DPRINT ("net_dns_get_ip (%s, %d, %p)\n", host, timeout, ipaddr);
    find_ip_t   fip = { STATE_WAITING, 0 };
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    err_t err = dns_gethostbyname (host, &fip.ipaddr, net_dns_found_cb, &fip);
    DPRINT ("&fip = %p, err = %d\n", &fip, err);
    if ( err == ERR_INPROGRESS )
        {
        while (( fip.err == STATE_WAITING ) && ( net_continue () ))
            {
            net_wait ();
            }
        DPRINT ("&fip = %p, fip.err = %d, fip.ipaddr = 0x%08X\n", &fip, fip.err, fip.ipaddr);
        if ( fip.err == STATE_WAITING )
            {
            DPRINT ("net_dns_get_ip: Timeout = %d\n", timeout);
            return net_error (ERR_TIMEOUT);
            }
        else if ( fip.err != ERR_OK )
            {
            DPRINT ("net_dns_get_ip: failed = %d\n", fip.err);
            return net_error (fip.err);
            }
        }
    else if ( err != ERR_OK )
        {
        DPRINT ("net_dns_get_ip: err = %d\n", err);
        return net_error (err);
        }
    memcpy (ipaddr, &fip.ipaddr, sizeof (ip_addr_t));
    DPRINT ("net_dns_get_ip: ipaddr = %s (0x%08X)\n", ipaddr_ntoa (ipaddr), *ipaddr);
    return ERR_OK;
    }

typedef struct s_udp_msg
    {
    struct pbuf         *p;
    ip_addr_t           addr;
    uint16_t            port;
    int                 nused;
    struct s_udp_msg    *next;
    } udp_msg_t;

static udp_msg_t    *umsg_free = NULL;

static udp_msg_t *udp_msg_alloc (void)
    {
    udp_msg_t   *m;
    if ( umsg_free != NULL )
        {
        m = umsg_free;
        umsg_free = m->next;
        }
    else
        {
        m = (udp_msg_t *) heap_calloc (1, sizeof (udp_msg_t));
        }
    return m;
    }

static void udp_msg_free (udp_msg_t *m)
    {
    m->next = umsg_free;
    umsg_free = m;
    }

typedef struct s_udp_conn
    {
    struct udp_pcb      *pcb;
    udp_msg_t           *m_fst;
    udp_msg_t           *m_lst;
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
        conn = (udp_conn_t *) heap_calloc (1, sizeof (udp_conn_t));
        }
    if ( conn != NULL )
        {
        conn->next = uconn_active;
        uconn_active = conn;
        }
    DPRINT ("udp_conn_alloc () = %p\n", conn);
    return conn;
    }

static void udp_conn_free (udp_conn_t *conn)
    {
    DPRINT ("udp_conn_free (%p)\n", conn);
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

static struct pbuf *pbuf_tail (struct pbuf *p)
    {
    while (p->next != NULL)
        {
        p = p->next;
        }
    return p;
    }

static void net_udp_receive_cb (void *connin, struct udp_pcb *pcb, struct pbuf *p,
    const ip_addr_t *addr, uint16_t port)
    {
#if DIAG
    DPRINT ("net_udp_receive_cb (%p, %p, %p, %p, %d)\n", connin, pcb, p, addr, port);
    DPRINT ("p->len = %d, p->tot_len = %d\n", p->len, p->tot_len);
    uint8_t *pd = (uint8_t *) p->payload;
    int n = p->len;
    while (n > 0)
        {
        // for (int i = 0; (i < 16) && (i < n); ++i) DPRINT ("%02X ", pd[i]);
        for (int i = 0; (i < 16) && (i < n); ++i) DPRINT ("%c", ((pd[i] >= ' ') && (pd[i] < 0x7F)) ? pd[i] : '.');
        DPRINT ("\n");
        n -= 16;
        pd += 16;
        }
#endif
    udp_conn_t *conn = (udp_conn_t *) connin;
    udp_msg_t *m = udp_msg_alloc ();
    m->p = p;
    m->addr = *addr;
    m->port = port;
    if ( conn->m_fst == NULL )
        {
        conn->m_fst = m;
        conn->m_lst = m;
        }
    else
        {
        conn->m_lst->next = m;
        conn->m_lst = m;
        }
    }

intptr_t net_udp_open (void)
    {
    DPRINT ("net_udp_open ()\n");
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
    DPRINT ("net_udp_bind (%p, %p, %d)\n", connin, ipaddr, port);
    if ( ! net_udp_valid (connin) ) return net_error (ERR_ARG);
    udp_conn_t *conn = (udp_conn_t *) connin;
    cyw43_arch_lwip_begin();
    err_t err = udp_bind (conn->pcb, ipaddr, port);
    cyw43_arch_lwip_end();
    return net_error (err);
    }

int net_udp_send (intptr_t connin, uint32_t len, void *data, ip_addr_t *ipaddr, uint32_t port)
    {
    DPRINT ("net_udp_send (%p, %d, %p, %p, %d)\n", connin, len, data, ipaddr, port);
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
    if ( conn->m_fst == NULL ) return 0;
    DPRINT ("net_udp_recv (%p, %d, %p, %p, %d)\n", connin, len, data, ipaddr, port);
    udp_msg_t *m = conn->m_fst;
    conn->m_fst = m->next;
    int nrecv = m->p->tot_len;
    struct pbuf *p = m->p;
    while (( len > 0 ) && ( p != NULL ))
        {
        DPRINT ("p->len = %d, p->tot_len = %d\n", p->len, p->tot_len);
        int ndata = p->len;
        if ( ndata > len ) ndata = len;
        memcpy (data, (uint8_t *)p->payload, ndata);
#if DIAG
        uint8_t *pd = (uint8_t *) data;
        for (int i = 0; i < ndata; ++i) DPRINT ("%c", ((pd[i] >= ' ') && (pd[i] <= '~')) ? pd[i] : '.');
        DPRINT ("\n");
#endif
        data += ndata;
        len -= ndata;
        p = p->next;
        }
    memcpy (ipaddr, &m->addr, sizeof (ip_addr_t));
    ISTORE (port, m->port);
    cyw43_arch_lwip_begin();
    pbuf_free (m->p);
    cyw43_arch_lwip_end();
    udp_msg_free (m);
#if DIAG
    if (nrecv > 0) DPRINT ("nrecv = %d\n", nrecv);
#endif
    return nrecv;
    }

void net_udp_close (intptr_t connin)
    {
    DPRINT ("net_udp_close (%p)\n", connin);
    if ( ! net_udp_valid (connin) ) return;
    udp_conn_t *conn = (udp_conn_t *) connin;
    udp_msg_t *m = conn->m_fst;
    cyw43_arch_lwip_begin();
    while (m != NULL)
        {
        if (m->p != NULL) pbuf_free (m->p);
        udp_msg_t *m2 = m->next;
        udp_msg_free (m);
        m = m2;
        }
    udp_disconnect (conn->pcb);
    udp_remove (conn->pcb);
    cyw43_arch_lwip_end();
    udp_conn_free (conn);
    }

void net_freeall (void)
    {
    DPRINT ("net_freeall ()\n");
    while ( tconn_active != NULL )
        {
        net_tcp_close ((intptr_t) tconn_active);
        }
    while ( tconn_free != NULL )
        {
        tcp_conn_t *conn = tconn_free;
        tconn_free = conn->next;
        heap_free ((void *) conn);
        }
    while ( uconn_active != NULL )
        {
        net_udp_close ((intptr_t) uconn_active);
        }
    while ( uconn_free != NULL )
        {
        udp_conn_t *conn = uconn_free;
        uconn_free = conn->next;
        heap_free ((void *) conn);
        }
    while ( umsg_free != NULL )
        {
        udp_msg_t *umsg = umsg_free;
        umsg_free = umsg->next;
        heap_free (umsg);
        }
    last_error = 0;
    }

int net_heap_size (int nconn)
    {
#if NET_HEAP
    int count = 0;
    int size = 0;
    memp_size (&count, &size);
    count += NUM_SCAN_RESULT + (2 + NUM_UDP_MSG) * nconn;
    size += NUM_SCAN_RESULT * sizeof (scan_cb_result_t)
        + nconn * sizeof (tcp_conn_t)
        + nconn * sizeof (udp_conn_t)
        + NUM_UDP_MSG * nconn * sizeof (udp_msg_t)
        + MALLOC_OVERHEAD * count;
    DPRINT ("net_heap_size = %d\n", size);
    return size;
#else
    return 0;
#endif
    }

void net_init (void *base, void *top)
    {
    DPRINT ("net_init (%p, %p)\n", base, top);
#if NET_HEAP
    if ( heap_init (base, top) )
        {
        tconn_active = NULL;
        tconn_free = NULL;
        uconn_active = NULL;
        uconn_free = NULL;
        umsg_free = NULL;
        }
#endif
    }

void net_term (void)
    {
    DPRINT ("net_term\n");
    net_freeall ();
#if NET_HEAP
    heap_term ();
#endif
    }

void net_limits (void **bot, void **top)
    {
#if NET_HEAP
    heap_limits (bot, top);
    DPRINT ("net_limits: %p, %p\n", bot, top);
#else
    *bot = NULL;
    *top = NULL;
#endif
    }
