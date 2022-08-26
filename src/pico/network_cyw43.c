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
static int scan_err = NET_ERR_NONE;

static int net_scan_cb (void *env, const cyw43_ev_scan_result_t *result)
    {
    if ( ( scan_err == NET_ERR_NONE ) && result )
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
            scan_err = NET_ERR_NO_MEM;
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
        if ( err != 0 ) return err - 200;
        scan_err = NET_ERR_NONE;
        bScan = true;
        }
    while ( ( scan_first == NULL ) && ( scan_err == NET_ERR_NONE )
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
        return NET_ERR_NONE;
        }
    memset (pscan, 0, sizeof (net_scan_result_t));
    bScan = false;
    if ( scan_err == NET_ERR_NONE ) scan_err = NET_ERR_DATA_END;
    return scan_err;
    }

typedef struct s_tcp_conn
    {
    struct tcp_pcb  *pcb;
    union
        {
        struct pbuf     *p;
        struct tcp_pcb  *acc;
        };
    int             nused;
    int             ndata;
    volatile int    err;
    } tcp_conn_t;

void net_tcp_err_cb (void *connin, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->err = err - 200;
    }

err_t net_tcp_connected_cb (void *connin, struct tcp_pcb *pcb, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( err == ERR_OK )    conn->err = STATE_COMPLETED;
    else                    conn->err = err - 200;
    return ERR_OK;
    }

err_t net_tcp_receive_cb (void *connin, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( p == NULL )
        {
        conn->err = NET_ERR_PEER_CLOSED;
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

intptr_t net_tcp_connect (uint32_t ipaddr, uint32_t port, uint32_t timeout)
    {
    tcp_conn_t *conn = (tcp_conn_t *) malloc (sizeof (tcp_conn_t));
    if ( conn == NULL ) return NET_ERR_NO_MEM;
    conn->pcb = tcp_new ();
    if ( conn->pcb == NULL )
        {
        free (conn);
        return NET_ERR_NO_MEM;
        }
    conn->p = NULL;
    conn->ndata = 0;
    conn->nused = 0;
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    conn->err = STATE_WAITING;
    tcp_arg (conn->pcb, conn);
    tcp_err (conn->pcb, net_tcp_err_cb);
    cyw43_arch_lwip_begin();
    tcp_connect (conn->pcb, (ip_addr_t *) &ipaddr, port, net_tcp_connected_cb);
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
        free (conn);
        if ( conn->err == STATE_WAITING ) return NET_ERR_TIMEOUT;
        return err;
        }
    tcp_recv (conn->pcb, net_tcp_receive_cb);
    return  (intptr_t) conn;
    }

err_t net_tcp_accept_cb (void *connin, struct tcp_pcb *pcb, err_t err)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->acc == NULL )
        {
        conn->acc = pcb;
        return ERR_OK;
        }
    else
        {
        tcp_abort (pcb);
        return ERR_ABRT;
        }
    }

intptr_t net_tcp_listen (uint32_t ipaddr, uint32_t port)
    {
    tcp_conn_t *conn = (tcp_conn_t *) malloc (sizeof (tcp_conn_t));
    if ( conn == NULL ) return NET_ERR_NO_MEM;
    conn->pcb = tcp_new ();
    if ( conn->pcb == NULL )
        {
        free (conn);
        return NET_ERR_NO_MEM;
        }
    conn->acc = NULL;
    conn->err = STATE_WAITING;
    tcp_arg (conn->pcb, conn);
    tcp_err (conn->pcb, net_tcp_err_cb);
    cyw43_arch_lwip_begin();
    err_t err = tcp_bind (conn->pcb, (ip_addr_t *) &ipaddr, port);
    cyw43_arch_lwip_end();
    if ( err == ERR_OK )
        {
        tcp_accept (conn->pcb, net_tcp_accept_cb);
        cyw43_arch_lwip_begin();
        struct tcp_pcb *pcb = tcp_listen (conn->pcb);
        cyw43_arch_lwip_end();
        if ( pcb == NULL )  err = NET_ERR_NO_MEM;
        else                conn->pcb = pcb;
        }
    if ( err != ERR_OK )
        {
        cyw43_arch_lwip_begin();
        tcp_abort (conn->pcb);
        cyw43_arch_lwip_end();
        free (conn);
        return err;
        }
    return (intptr_t) conn;
    }

intptr_t net_tcp_accept (intptr_t listen)
    {
    tcp_conn_t *conn = (tcp_conn_t *) listen;
    if ( conn->acc == NULL ) return 0;
    struct tcp_pcb *pcb = conn->acc;
    conn->acc = NULL;
    err_t err = NET_ERR_NONE;
    tcp_conn_t *nconn = (tcp_conn_t *) malloc (sizeof (tcp_conn_t));
    if ( nconn == NULL )
        {
        cyw43_arch_lwip_begin();
        tcp_abort (pcb);
        cyw43_arch_lwip_end();
        return NET_ERR_NO_MEM;
        }
    nconn->pcb = pcb;
    nconn->p = NULL;
    nconn->ndata = 0;
    nconn->nused = 0;
    tcp_arg (nconn->pcb, nconn);
    tcp_err (nconn->pcb, net_tcp_err_cb);
    tcp_recv (nconn->pcb, net_tcp_receive_cb);
    return (intptr_t) nconn;
    }

err_t net_tcp_sent_cb (void *connin, struct tcp_pcb *pcb, uint16_t len)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    conn->ndata = len;
    conn->err = STATE_COMPLETED;
    return ERR_OK;
    }

int net_tcp_write (intptr_t connin, uint32_t len, void *addr, uint32_t timeout)
    {
    if ( connin < 0 ) return connin;
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->err != NET_ERR_NONE ) return conn->err;
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    while ( len > 0 )
        {
        conn->ndata = tcp_sndbuf (conn->pcb);
        if ( conn->ndata > 0 )
            {
            uint8_t flags = TCP_WRITE_FLAG_MORE;
            if ( conn->ndata >= len )
                {
                flags = 0;
                conn->ndata = len;
                }
            conn->err = STATE_WAITING;
            tcp_sent (conn->pcb, net_tcp_sent_cb);
            cyw43_arch_lwip_begin();
            tcp_write (conn->pcb, addr, conn->ndata, flags);
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
                return conn->err;
                }
            addr += conn->ndata;
            len -= conn->ndata;
            }
        }
    return NET_ERR_NONE;
    }

int net_tcp_read (intptr_t connin, uint32_t len, void *addr, uint32_t timeout)
    {
    if ( connin < 0 ) return connin;
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    if ( conn->err != NET_ERR_NONE ) return conn->err;
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    while (( conn->p == NULL ) && net_continue () )
        {
        net_wait ();
        }
    if ( conn->err != STATE_COMPLETED )
        {
        cyw43_arch_lwip_begin();
        tcp_abort (conn->pcb);
        cyw43_arch_lwip_end();
        return conn->err;
        }
    if ( conn->p == NULL ) return NET_ERR_TIMEOUT;
    int nrecv = 0;
    struct pbuf *p = conn->p;
    while (( len > 0 ) && ( p != NULL ))
        {
        int ndata = p->len - conn->nused;
        if ( ndata > len ) ndata = len;
        memcpy (addr, (uint8_t *)p->payload + conn->nused, ndata);
        nrecv += ndata;
        addr += ndata;
        len -= ndata;
        if ( conn->nused == p->len )
            {
            cyw43_arch_lwip_begin();
            tcp_recved (conn->pcb, p->len);
            cyw43_arch_lwip_end();
            conn->p = p->next;
            conn->nused = 0;
            pbuf_ref (p->next);
            pbuf_free (p);
            }
        }
    return nrecv;
    }

void net_tcp_close (intptr_t connin)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    while ( true )
        {
        cyw43_arch_lwip_begin();
        err_t err = tcp_close (conn->pcb);
        cyw43_arch_lwip_end();
        if ( err == ERR_OK ) break;
        net_wait ();
        }
    free (conn);
    }

void net_tcp_peer (intptr_t connin, uint32_t *ipaddr, uint32_t *port)
    {
    tcp_conn_t *conn = (tcp_conn_t *) connin;
    ip_addr_t raddr;
    uint16_t rport;
    cyw43_arch_lwip_begin();
    tcp_tcp_get_tcp_addrinfo (conn->pcb, 0, &raddr, &rport);
    cyw43_arch_lwip_end();
    ISTORE (ipaddr, raddr.addr);
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

int net_dns_get_ip (const char *host, uint32_t timeout, uint32_t *ipaddr)
    {
    find_ip_t   fip = { false, 0 };
    if ( timeout == 0 ) net_tend = at_the_end_of_time;
    else                net_tend = make_timeout_time_ms (timeout);
    err_t err = dns_gethostbyname (host, &fip.ipaddr, net_dns_found_cb, &fip);
    if ( err != ERR_OK ) return err;
    while (( ! fip.found ) && ( net_continue () ))
        {
        net_wait ();
        }
    if ( fip.found )
        {
        ISTORE (ipaddr, fip.ipaddr.addr);
        err = NET_ERR_NONE;
        }
    else
        {
        err = NET_ERR_TIMEOUT;
        }
    return err;
    }
