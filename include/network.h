// network.h - Nework routines intended to be usable from within BBC BASIC on Pico W

#ifndef NETWORK_H
#define NETWORK_H

#include <cyw43.h>
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------

net_last_errno - Get last error number

SYS "net_last_errno" TO err%

err% =      Last error number.

*/

int net_last_errno (void);

/*-------------------------------------------------------------------------------

net_error_desc - Get the description of an error number

SYS "net_error_desc", err% TO desc%

err% =      Error number
desc% =     Pointer to description string

*/

const char *net_error_desc (int err);

/*-------------------------------------------------------------------------------

net_error_clear - Clear the last error code

SYS "net_error_clear"

*/

void net_error_clear (void);

/*-------------------------------------------------------------------------------

net_wifi_scan - Scan for available WiFi access points

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
err% =  ERR_OK (0) - Returned details of a station
        ERR_CLSD (-15) - No more stations
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

/*-------------------------------------------------------------------------------

net_wifi_get_state - Get address of cyw43_state structure

SYS "net_wifi_get_state" TO addr%

addr% =     Address of the state structure

*/

const cyw43_t *net_wifi_get_state (void);

/*-------------------------------------------------------------------------------

net_wifi_get_netif - Get address of network interface structures

SYS "net_wifi_get_netif", iface% TO addr%

iface% =    Interface number (0 or 1)
addr% =     Address of the interface structure
            NULL (0) - Invalid interface number

*/

const struct netif *net_wifi_get_netif (int iface);

/*-------------------------------------------------------------------------------

net_wifi_get_ipaddr - Get IP address of network interface

SYS "net_wifi_get_ipaddr", iface%, ^ipaddr% TO err%

iface% =    Interface number (0 or 1)
ipaddr% =   IP address of the interface
err% =      ERR_OK (0) - Address returned
            ERR_CONN (-11) - No address assigned
            ERR_ARG (-16) - Invalid interface number

*/

int net_wifi_get_ipaddr (int iface, ip_addr_t *ipaddr);

/*-------------------------------------------------------------------------------

net_tcp_connect - Open a TCP connection

SYS "net_tcp_connect", ^ipaddr%, port%, timeout% TO conn%

ipaddr% =   IP address.
port% =     Port number.
timeout% =  Timeout in ms. Zero for wait forever.
conn% >0 -  Connection handle
      <0 -  Error number

*/

intptr_t net_tcp_connect (const ip_addr_t *ipaddr, uint32_t port, uint32_t timeout);

/*-------------------------------------------------------------------------------

net_tcp_valid - Test for a valid TCP socket

SYS "net_tcp_valid" conn% TO valid%

conn% =     Connection handle
valid% =    1 - Valid connection
            0 - Invalid

*/

int net_tcp_valid (intptr_t connin);

/*-------------------------------------------------------------------------------

net_tcp_listen - Open a socket to listen for a connection on a specified address and port

SYS "net_tcp_listen", ^ipaddr%, port% to listen%

ipaddr% =       Local IP address (0 = all local addresses).
port% =         Port number
listen% >0 -    Listening socket
        <0 -    Error number

*/

intptr_t net_tcp_listen (const ip_addr_t *ipaddr, uint32_t port);

/*-------------------------------------------------------------------------------

net_tcp_accept - Accept a connection from a listening port

SYS "net_tcp_accept", listen% TO conn%

listen% =   Listening socket
conn% >0 -  Connection handle
      =0 -  No waiting connection.
      <0 -  Error number

*/

intptr_t net_tcp_accept (intptr_t listen);

/*-------------------------------------------------------------------------------

net_tcp_write - Write data to a TCP connection

SYS "net_tcp_write", conn%, len%, data%, timeout% TO err%

conn% =     TCP connection handle
len% =      Length of data to write
data% =     Address of data to write
timeout% =  Timeout in ms. Zero for wait forever.
err% =      Error status

*/

int net_tcp_write (intptr_t conn, uint32_t len, void *data, uint32_t timeout);

/*-------------------------------------------------------------------------------

net_tcp_read - Read data from a TCP connection

SYS "net_tcp_read", conn%, len%, data%, timeout% TO err%

conn% =     TCP connection handle
len% =      Length of buffer to receive data
data% =     Address of buffer to receive data
err% >0 -   Number of bytes received
     <0 -   Error status

*/

int net_tcp_read (intptr_t conn, uint32_t len, void *data);

/*-------------------------------------------------------------------------------

net_tcp_close - Close a TCP connection

The connection handle must not be used after calling this routine.

SYS "net_tcp_close", conn%

conn% =     TCP connection handle

*/

void net_tcp_close (intptr_t conn);

/*-------------------------------------------------------------------------------

net_tcp_peer - Get information on the peer of a connection

SYS "net_tcp_peer", conn%, ^ipaddr%, ^port%

conn% =     TCP connection handle.
ipaddr% =   Variable to receive IP address.
port% =     Variable to receive port number.

*/

void net_tcp_peer (intptr_t conn, ip_addr_t *ipaddr, uint32_t *port);

/*-------------------------------------------------------------------------------

net_dns_get_ip - Find the IP address corresponding to a host name

SYS "net_dns_get_ip", host$, timeout%, ^ipaddr% TO err%

host$ =     Host name to find corresponding IP address (zero terminated)
timeout% =  Timeout in ms. Zero to wait forever.
ipaddr% =   Variable to receive IP address.
err% =      Error status.

*/

int net_dns_get_ip (const char *host, uint32_t timeout, ip_addr_t *ipaddr);

/*-------------------------------------------------------------------------------

net_udp_open - Create a UDP connection

SYS "net_udp_open" TO conn%

conn% =     Connection handle

*/

intptr_t net_udp_open (void);

/*-------------------------------------------------------------------------------

net_udp_valid - Test for a valid UDP socket

SYS "net_udp_valid" conn% TO valid%

conn% =     Connection handle
valid% =    1 - Valid connection
            0 - Invalid

*/

int net_udp_valid (intptr_t connin);

/*-------------------------------------------------------------------------------

net_udp_bind - Bind a UDP connection to a local address and port

SYS "net_udp_bind", conn%, ^ipaddr%, port% TO err%

conn% =     UDP connection handle
ipaddr% =   Local IP address in network byte order (0 = all local addresses).
port% =     Port number in network byte order.
err% =      Error status

*/

int net_udp_bind (intptr_t conn, const ip_addr_t *ipaddr, uint32_t port);

/*-------------------------------------------------------------------------------

net_udp_send - Send a UDP packet to a remote address and port

SYS "net_udp_send", conn%, len%, data%, ^ipaddr%, port% TO err%

conn% =     TCP connection handle
len% =      Length of data to write
data% =     Address of data to write
ipaddr% =   Destination address
port% =     Destination port
err% =      Error status

*/

int net_udp_send (intptr_t conn, uint32_t len, void *data, ip_addr_t *ipaddr, uint32_t port);

/*-------------------------------------------------------------------------------

net_udp_recv - Receive a UDP packet from a remote address and port

SYS "net_udp_recv", conn%, len%, data%, ^ipaddr%, port% TO nrecv%

conn% =     TCP connection handle
len% =      Length of buffer to receive data
data% =     Address of buffer to receive data
ipaddr% =   Originating address
port% =     Originating port
nrecv% =    Length of received data (zero if none)

*/

int net_udp_recv (intptr_t conn, uint32_t len, void *data, ip_addr_t *ipaddr, uint32_t *port);

/*-------------------------------------------------------------------------------

net_freeall - Close all connections and free all memory

SYS "net_freeall"

*/

void net_freeall (void);

/*-------------------------------------------------------------------------------

net_heap size - Suggest size of heap for network connections

SYS "net_heap_size", nconn% TO size%

nconn% =    Expected number of connections

*/

int net_heap_size (int nconn);

/*-------------------------------------------------------------------------------

net_init - Initialise memory for network operations

SYS "net_init", base%, top%

base% =     Base of memory for heap
top% =      Top of memory for heap

*/

void net_init (void *base, void *top);

/*-------------------------------------------------------------------------------

net_term - Tidy up memory following network operations

SYS "net_term"

*/

void net_term (void);

/*-------------------------------------------------------------------------------

net_limits - Obtain position of network heap

SYS "net_limits", ^base%, ^top%

base% =     Base of memory for heap
top% =      Top of memory for heap

*/

void net_limits (void **base, void **top);

#endif
