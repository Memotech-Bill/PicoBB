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

DIM ssid&(32)
DIM mac&(5)
SYS "net_wifi_scan",  ^ssid&(0), ^rssi%, ^chan%, ^mac&(0), ^auth% TO err%

ssid& = Station identifier (zero terminated string)
rssi =  Signal strength
chan =  Channel number
mac =   MAC address (6 bytes)
auth =  Authorisation mode
err =   NET_ERR_NONE - Returned details of a station
        NET_ERR_DATA_END - No more stations
        Internal errors
*/

int net_wifi_scan (char *ssid, int *rssi, int *chan, uint8_t *mac, int *auth);

#endif
