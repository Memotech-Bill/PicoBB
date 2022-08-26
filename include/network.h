// network.h - Nework routines intended to be usable from within BBC BASIC on Pico W

#ifndef NETWORK_H
#define NETWORK_H

#define NET_ERR_NONE        0
#define NET_ERR_DATA_END    -1
#define NET_ERR_TIMEOUT     -2
#define NET_ERR_PEER_CLOSED -3
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

/* net_tcp_connect - Open a TCP connection

SYS "net_tcp_connect", ipaddr%, port%, timeout% TO conn%

ipaddr% =   IP address in network byte order.
port% =     Port number in network byte order.
timeout% =  Timeout in ms. Zero for wait forever.
conn% >0 -  Connection handle
      <0 -  Error number

*/

intptr_t net_tcp_connect (uint32_t ipaddr, uint32_t port, uint32_t timeout);

/* net_tcp_listen - Listen for a connection on a specified address and port

SYS "net_tcp_listen", ipaddr%, port% to listen%

ipaddr% =       IP address in network byte order.
port% =         Port number in network byte order.
listen% >0 -    Listening handle
        <0 -    Error number

*/

intptr_t net_tcp_listen (uint32_t ipaddr, uint32_t port);

/* net_tcp_accept - Accept a connection from a listening port

SYS "net_tcp_accept", listen% TO conn%

listen% =   Listening handle.
conn% >0 -  Connection handle
      =0 -  No waiting connection.
      <0 -  Error number

*/

intptr_t net_tcp_accept (intptr_t listen);

/* net_tcp_write - Write data to a TCP connection

SYS "net_tcp_write", conn%, len%, addr%, timeout% TO err%

conn% =     TCP connection handle
len% =      Length of data to write
addr% =     Address of data to write
timeout% =  Timeout in ms. Zero for wait forever.
err% =      Error status

*/

int net_tcp_write (intptr_t conn, uint32_t len, void *addr, uint32_t timeout);

/* net_tcp_read - Read data from a TCP connection

SYS "net_tcp_read", conn%, len%, addr%, timeout% TO err%

conn% =     TCP connection handle
len% =      Length of buffer to receive data
addr% =     Address of buffer to receive data
timeout% =  Timeout in ms. Zero for wait forever.
err% >0 -   Number of bytes received
     <0 -   Error status

*/

int net_tcp_read (intptr_t conn, uint32_t len, void *addr, uint32_t timeout);

/* net_tcp_close - Close a TCP connection

The connection handle must not be used after calling this routine.

SYS "net_tcp_close", conn%

conn% =     TCP connection handle

*/

void net_tcp_close (intptr_t conn);

/* net_tcp_peer - Get information on the peer of a connection

SYS "net_tcp_peer", conn%, ^ipaddr%, ^port%

conn% =     TCP connection handle.
ipaddr% =   Variable to receive IP address.
port% =     Variable to receive port number.

*/

void net_tcp_peer (intptr_t conn, uint32_t *ipaddr, uint32_t *port);

/* net_dns_get_ip - Find the IP address corresponding to a host name

SYS "net_dns_get_ip", host$, timeout%, ^ipaddr% TO err%

host$ =     Host name to find corresponding IP address (zero terminated)
timeout% =  Timeout in ms. Zero to wait forever.
ipaddr% =   Variable to receive IP address.
err% =      Error status.

*/

int net_dns_get_ip (const char *host, uint32_t timeout, uint32_t *ipaddr);

#endif
