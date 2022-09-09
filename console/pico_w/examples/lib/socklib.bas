REM Socket library for BBC BASIC for SDL 2.0, R.T.Russell, v2.3, 25-Aug-2020
REM UDP support additions adapted from proposals by R.P.D (Phil) 30-Nov-2017
REM Principal public routines are:
REM  PROC_initsockets            Initialise the BBCSDL socket interface
REM  PROC_exitsockets            Close down the BBCSDL socket interface
REM  FN_gethostname              Return the name of the local host (string)
REM  FN_tcplisten(host$,port$)   Create a TCP/IP listening socket
REM  FN_tcpconnect(host$,port$)  Make a connection and return socket
REM  FN_checkconnection(socket)  Check for an incoming connection on socket
REM  FN_writesocket(skt,buf,len) Write 'len' bytes at addr 'buf' to 'skt'
REM  FN_readsocket(skt,buf,len)  Read max 'len' bytes to addr 'buf' from 'skt'
REM  PROC_closesocket(socket)    Close a socket
REM  FN_socketerror              Return last socket error number
REM  FN_socketerror$             Return last socket error string
REM  FN_writelinesocket(skt,s$)  Write 's$' followed by CRLF to 'skt'
REM  FN_readlinesocket(skt,t,s$) Read a line of text to 's$' with timeout t cs
REM  FN_getpeername(socket)      Return IPV4 address of remote machine
REM Routines added or made public in v2.1:
REM  FN_sethost(host$)           Return the host address in network byte order
REM  FN_setport(port$)           Return the port number in network byte order
REM  FN_checkconnectionM(socket) As FN_checkconnection but don't close listen skt
REM  FN_udpsocket(host$,port$)   Create a UDP socket and optionally bind to host
REM  FN_sendtosocket(skt,buf,len,host,port)   Write to UDP socket, specifying peer
REM  FN_recvfromsocket(skt,buf,len,host,port) Read from UDP socket, returning peer

REM Initialise the BBCSDL Sockets interface
DEF PROC_initsockets(N%)
DEF PROC_initsockets
LOCAL chan%, err%
SYS "net_freeall"
chan% = OPENIN("wifi.cfg")
IF chan% = 0 THEN
LOCAL ccode$, cc1%, cc2%
INPUT "SSID", ssid$, "Password", pwd$, "Country Code (2 letters)", ccode$
cc1% = ASC(LEFT$(ccode$, 1))
cc2% = ASC(MID$(ccode$, 2, 1))
IF (cc1% >= 97) AND (cc1% <= 122) THEN cc1% -= 32
IF (cc2% >= 97) AND (cc2% <= 122) THEN cc2% -= 32
ccode% = cc1% + 256 * cc2%
IF ccode% = 19285 THEN ccode% = 16967 : REM UK -> GB
chan = OPENOUT("wifi.cfg")
PRINT#chan, ssid$, pwd$, ccode%
ELSE
INPUT#chan%, ssid$, pwd$, ccode%
ENDIF
CLOSE#chan%
SYS "cyw43_arch_init_with_country", ccode% TO err%
IF err% <> 0 THEN
ERROR 195, "Error "+STR$(err%)+" initialising WiFi"
END
ENDIF
SYS "cyw43_arch_enable_sta_mode"
SYS "cyw43_arch_wifi_connect_timeout_ms", ssid$, pwd$, &400004, 30000 TO err%
IF err% <> 0 THEN
ERROR 196, "Error "+STR$(err%)+" connecting to access point"
END
ENDIF
ENDPROC

REM Shut down the BBCSDL Sockets interface
DEF PROC_exitsockets
SYS "net_freeall"
ENDPROC

REM Get the name of the local machine
DEF FN_gethostname
LOCAL h%, IPaddress{} : DIM IPaddress{host%}
230 SYS "net_wifi_get_ipaddr", 0, ^IPaddress.host% TO err%
240 IF err% <> 0 THEN = ""
280 SYS "ip4addr_ntoa", ^IPaddress.host% TO h%
= $$h%

REM Set up a TCP listening socket
DEF FN_tcplisten(host$,port$)
LOCAL S%, IPaddress{} : DIM IPaddress{host%, port%}
IPaddress.host% = 0
IPaddress.port% = FN_setport(port$)
IF IPaddress.port% = 0 THEN = -1
SYS "net_tcp_listen", ^IPaddress.host%, IPaddress.port% TO S%
= S%

REM Make a TCP connection
DEF FN_tcpconnect(host$,port$)
LOCAL S%, IPaddress{} : DIM IPaddress{host%, port%}
IPaddress.host% = FN_sethost(host$)
IF IPaddress.host% = FALSE OR IPaddress.host% = TRUE THEN = -1
IPaddress.port% = FN_setport(port$)
IF IPaddress.port% = 0 THEN = -1
SYS "net_tcp_connect", ^IPaddress.host%, IPaddress.port%, 10000 TO S%
= S%

REM Check for an incoming connection on a listening socket
DEF FN_check_connectionM(S%):LOCAL D%
DEF FN_check_connection(S%):LOCAL D%:D%=TRUE
LOCAL sock%
SYS "net_tcp_accept", S% TO sock%
IF sock% = 0 THEN = 0
IF D% OR (sock% < 0) THEN
SYS "net_tcp_close", S%
ENDIF
= sock%

REM Write to a TCP socket (S% = socket, buff%% = buffer address, L% = length)
DEF FN_writesocket(S%,buff%%,L%)
LOCAL E%, B%
B% = buff%%
SYS "net_tcp_write", S%, L%, B%, 10000 TO E%
IF E% <= 0 PROC_closesocket(S%) : = TRUE : REM Connection closed or error
= E%

REM Read from a TCP socket (S% = socket, buff%% = buffer address, L% = buffer size)
DEF FN_readsocket(S%,buff%%,L%)
LOCAL N%, B%
B% = buff%%
SYS "net_tcp_read", S%, L%, B% TO N%
IF N% < 0 PROC_closesocket(S%) : = TRUE : REM Connection closed or error
= N%

REM Create a UDP socket and optionally bind to host
DEF FN_udpsocket(host$,port$)
LOCAL S%, IPaddress{} : DIM IPaddress{host%, port%}
SYS "net_udp_open" TO S%
IF S% = 0 THEN = S%
IF host$<>"" IPaddress.host% = FN_sethost(host$)
IF port$<>"" IPaddress.port% = FN_setport(port$)
IF IPaddress.port% > 0 THEN
SYS "net_udp_bind", S%, ^IPaddress.host%, IPaddress.port% TO E%
IF E% < 0 THEN
SYS "net_udp_close", S%
= E%
ENDIF
ENDIF
= S%

REM Write to bound or unbound UDP socket, specifying peer's address
REM S%=socket, buff%%=buffer address, L%=buffer size, H%=host IP, P%=host port
DEF FN_sendtosocket(S%,buff%%,L%,H%,P%)
LOCAL B%, N%, host{} : DIM host{addr%}
B% = buff%%
host.addr% = H%
SYS "net_udp_send", S%, L%, B%, ^host.addr%, P% TO N%
= N%

REM Read from bound UDP socket, returning peer's address
REM S%=socket, buff%%=buffer address, L%=buffer size, H%=host IP, P%=host port
DEF FN_recvfromsocket(S%,buff%%,L%,RETURN H%,RETURN P%)
LOCAL B%, N%, host{} : DIM host{addr%, port%}
B% = buff%%
SYS "net_udp_recv", S%, L%, B%, ^host.addr%, ^host.port% TO N%
H% = host.addr%
P% = host.port%
= N%

REM Close a TCP or UDP socket
DEF PROC_closesocket(S%)
LOCAL T%
SYS "net_tcp_valid", S% TO T%
IF T% THEN
SYS "net_tcp_close", S%
ELSE
SYS "net_udp_close", S%
ENDIF
ENDPROC

REM Return last socket error number (actually LS 32-bits of a string pointer)
DEF FN_socketerror : LOCAL E%
SYS "net_last_errno" TO E%
= E%

REM Return last socket error as a string
DEF FN_socketerror$ : LOCAL e%
SYS "net_last_errno" TO e%
SYS "net_error_desc", e% TO e%
= $$e%

REM Set the host address
DEF FN_sethost(host$)
LOCAL H%,I%,P%, IPaddress{} : DIM IPaddress{host%, port%}
IF VAL(host$) THEN
FOR I% = 1 TO 4
H% = (H% >>> 8) OR (VAL(host$) << 24)
P% = INSTR(host$, ".")
host$ = MID$(host$, P%+1)
NEXT
ELSE
SYS "net_dns_get_ip", PTR(host$), 10000, ^IPaddress.host% TO I%
IF I% = 0 THEN H% = IPaddress.host% ELSE H% = FALSE
ENDIF
= H%

REM Set the port number
DEF FN_setport(port$)
LOCAL P%
CASE port$ OF
WHEN "echo": P% = 7
WHEN "discard": P% = 9
WHEN "systat": P% = 11
WHEN "daytime": P% = 13
WHEN "netstat": P% = 15
WHEN "chargen": P% = 19
WHEN "ftp": P% = 21
WHEN "telnet": P% = 23
WHEN "smtp": P% = 25
WHEN "time": P% = 37
OTHERWISE: P% = VAL(port$)
ENDCASE
= P% AND &FFFF

REM Write string A$ followed by CRLF to socket S%
DEF FN_writelinesocket(S%,A$)
LOCAL E%
A$ += CHR$13+CHR$10
REPEAT
E% = FN_writesocket(S%,PTR(A$),LENA$)
IF E%=0 WAIT 0
IF E%<0 THEN = E%
A$ = MID$(A$,E%+1)
UNTIL A$=""
= 0

REM Read a line of text to A$ from socket S% with timeout T% centiseconds
DEF FN_readlinesocket(S%,T%,RETURN A$)
LOCAL buff%%,E%,I%
PRIVATE B$
DIM buff%% LOCAL 255
ON ERROR LOCAL RESTORE LOCAL : ERROR ERR,REPORT$
I%=INSTR(B$,CHR$10)
IF I%=0 THEN
T% += TIME
REPEAT
E% = FN_readsocket(S%,buff%%,256)
IF E%=0 WAIT 0
IF E%>0 FOR I%=0 TO E%-1:B$ += CHR$buff%%?I% : NEXT
I% = INSTR(B$,CHR$10)
UNTIL E%<0 OR I%<>0 OR TIME>T%
ENDIF
IF E%<0 THEN =E%
IF I% A$=LEFT$(B$,I%-1) : B$ = MID$(B$,I%+1) ELSE A$=""
IF RIGHT$(A$)=CHR$13 A$ = LEFT$(A$)
IF ASCB$=13 B$ = MID$(B$,2)
=LENA$

REM  Return IP address of machine connected via socket S%
DEF FN_getpeername(S%)
LOCAL name%, peer{} : DIM peer{addr%, port%}
SYS "net_tcp_peer", S%, ^peer.addr%, ^peer.port%
SYS "ip4addr_ntoa", ^peer.addr% TO name%
= $$name%
