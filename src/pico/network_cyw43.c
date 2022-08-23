// net_cyw43.c - Network support for Pico W

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "BBC.h"
#include "network.h"
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <hardware/sync.h>

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
