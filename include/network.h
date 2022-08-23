// network.h - Nework routines intended to be usable from within BBC BASIC on Pico W

#ifndef NETWORK_H
#define NETWORK_H

#define NET_ERR_NONE        0
#define NET_ERR_DATA_END    -1
#define NET_ERR_NO_MEM      -101

#include <stdint.h>

/* net_wifi_scan - Scan for available WiFi access points

The first call to this routine (or another call after a non-zero error return)
will start a new WiFi scan. Each successfull call will return another access point.
The call following the last access point will return NET_ERR_DATA_END and all zero
data.

DIM net{rssi%, chan%, auth%, ssid&(32), mac&(5)}
SYS "net_wifi_scan", net{} TO err%

rssi% = Signal strength
auth% = Authorisation mode
chan% = Channel number
ssid& = Station identifier (zero terminated string)
mac& =  MAC address (6 bytes)
err% =  NET_ERR_NONE - Returned details of a station
        NET_ERR_DATA_END - No more stations
        Internal errors
*/

typedef struct s_net_scan_result
    {
    uint32_t    rssi;
    uint32_t    chan;
    uint32_t    auth;
    char        ssid[33];
    uint8_t     mac[6];
    } net_scan_result_t;

int net_wifi_scan (net_scan_result_t *pscan);

#endif
