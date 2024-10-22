REM Program to demonstrate the 'mysqllib' library for 'BBC BASIC for SDL 2.0'
REM (should also work with 'BBC BASIC for Windows'). R.T.Russell 18-Aug-2022

REM SERV$ = "mysql-rfam-public.ebi.ac.uk"
SERV$ = "193.62.194.222"
PORT$ = "4497"
USER$ = "rfamro"
PASS$ = ""
DB$   = "Rfam"
TB$   = "author"

MAXROWS = 14
MAXWIDE = 22

ON ERROR IF ERR = 17 CHAIN @lib$ + "../examples/tools/touchide" ELSE               PRINT REPORT$ " at line "; ERL : END

VDU 23,22,800;600;8,17,16,128

REM INSTALL @lib$ + "mysqllib"
REM INSTALL @lib$ + "socklib"
PROC_initsockets

REM Connect to MySQL:
PRINT "Connecting to MySQL server '" SERV$ "'... ";
sock% = FN_tcpconnect(SERV$, PORT$)
IF sock% > 0 THEN
PRINT "connected successfully"
ELSE
IF sock% <= 0 PRINT "failed to connect, aborting"
PROC_exitsockets
END
ENDIF
PRINT

REM Connect to database:
PRINT "Connecting to database '" DB$ "'... ";
REPEAT
res% = FN_sqlConnect(sock%, USER$, PASS$, DB$, version$)
IF res% = -12 THEN
PROC_closesocket(sock%)
sock% = FN_tcpconnect(SERV$, PORT$)
ENDIF
UNTIL res% <> -12

IF res% = 0 THEN
PRINT "connected successfully"
ELSE
IF res% PRINT "failed to connect, code = "; res%
IF res% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF
PRINT "MySQL version = "; version$ '

REM Enumerate databases:
PRINT "Fetching database names... ";
res% = FN_sqlQuery(sock%, "SHOW DATABASES")
IF res% < 0 THEN
PRINT "query failed, code = "; res%
IF res% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF

IF res% THEN
DIM dbname$(res%), dbwide%(res%)
ncol% = FN_sqlColumns(sock%, dbname$(), dbwide%())
DIM dbase$(MAXROWS,ncol%)
nrow% = FN_sqlFetch(sock%, dbase$())
PRINT ;nrow% " database(s) enumerated:";
FOR I% = 1 TO nrow%
PRINT " '" dbase$(I%,1) "'";
NEXT
ENDIF
PRINT '

REM Enumerate tables in current database:
PRINT "Fetching table names... ";
res% = FN_sqlQuery(sock%, "SHOW TABLES")
IF res% < 0 THEN
PRINT "query failed, code = "; res%
IF res% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF

IF res% THEN
DIM tbname$(res%), tbwide%(res%)
ncol% = FN_sqlColumns(sock%, tbname$(), tbwide%())
DIM table$(MAXROWS,ncol%)
nrow% = FN_sqlFetch(sock%, table$())
PRINT ;nrow% " table(s) enumerated:";
FOR I% = 1 TO nrow%
PRINT " '" table$(I%,1) "'";
NEXT
ENDIF
PRINT '

REM Fetch table:
PRINT "Querying database '" DB$ "' for table '" TB$ "'..."
ncol% = FN_sqlQuery(sock%, "select * from " + TB$)
IF ncol% < 0 THEN
PRINT "Query failed, code = "; ncol%
IF ncol% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF
PRINT

REM Fetch column names and widths (if any):
PRINT "Fetching column titles... ";
DIM name$(ncol%), wide%(ncol%)
num% = FN_sqlColumns(sock%, name$(), wide%())
IF num% >= 0 THEN
PRINT "number of columns = "; num%
ELSE
PRINT "failed to fetch columns, code = "; res%
IF num% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF

REM Fetch table data:
PRINT "Fetching table data... ";
DIM array$(MAXROWS, ncol%)
nrow% = FN_sqlFetch(sock%, array$())
IF nrow% >= 0 THEN
PRINT "number of rows read = "; nrow%
ELSE
IF nrow% < 0 PRINT "failed to fetch database, code = "; nrow%
IF nrow% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF
PRINT

REM Print the table heading:
T% = 0
IF num% THEN
FOR C% = 1 TO num%
PRINT TAB(T%) name$(C%);
W% = wide%(C%) : IF W% > MAXWIDE W% = MAXWIDE
T% += W%
NEXT
ENDIF
PRINT

REM Print the returned database:
FOR R% = 1 TO nrow%
T% = 0
FOR C% = 1 TO ncol%
PRINT TAB(T%) array$(R%, C%);
W% = wide%(C%) : IF W% > MAXWIDE W% = MAXWIDE
T% += W%
NEXT
PRINT
NEXT
PRINT

PROC_closesocket(sock%)
PROC_exitsockets

PRINT "Transaction completed successfully";
REPEAT WAIT 4 : UNTIL FALSE

REM Client library to access a MySQL database from 'BBC BASIC for SDL 2.0'
REM By R. T. Russell, http://www.rtrussell.co.uk, version 0.0, 16-Jun-2022
REM The calling program must install 'socklib' in addition to this library

REM Test message for validation:
REM message$ = "The quick brown fox jumps over the lazy dog"
REM PRINT "2FD4E1C67A2D28FCED849EE1BB76E7391B93EB12"
REM sha1$ = FN_sha1(message$)
REM FOR I% = 1 TO 20 : PRINT RIGHT$("0"+STR$~ASCMID$(sha1$,I%),2);:NEXT:PRINT

REM Connect to a MySQL database, returns zero on success or a failure code:
REM -1: Header read fail, -2: Header read timeout, -3 Unexpected sequence id
REM -4: Payload too large, -5: Payload read fail, -6: Payload read timeout
REM -7: Protocol version unsupported, -8: Authentication method unsupported
REM -9: Unexpected auth1 string length, -10: Unexpected auth2 string length
REM -11: Failed to write to socket, -12: Retry connection, -13: MySQL error
REM -14: Authorisation method not supported, -15: Unexpected packet type
DEF FN_sqlConnect(S%, user$, pass$, db$, RETURN version$)
LOCAL D%, L%, P%, R%, T%, U%, auth1$, auth2$, pwd$, method$, response&(), command&()
DIM response&(255)

REM Read initial handshake packet:
R% = FN_sqlPacket(S%, 0, response&())
IF R% < 0 THEN = R%
IF response&(0) <> 10 THEN = -7

version$ = $$^response&(1)
T% = 1 + LEN(version$)

REM PRINT "Capability flags (lo) = &"; ~response&(T% + 14) + 256 * response&(T% + 15)
REM PRINT "Capability flags (hi) = &"; ~response&(T% + 19) + 256 * response&(T% + 20)

method$ = $$^response&(T% + 45)
IF method$ <> "mysql_native_password" THEN = -8
auth1$ = $$^response&(T% + 5)
IF LEN(auth1$) <> 8 THEN = -9
auth2$ = $$^response&(T% + 32)
IF LEN(auth2$) <> 12 THEN = -10

IF pass$ = "" THEN
pwd$ = ""
ELSE
pwd$ = FN_sxor(FN_sha1(pass$), FN_sha1(auth1$ + auth2$ + FN_sha1(FN_sha1(pass$))))
ENDIF

IF INSTR(pwd$, CHR$0) THEN = -12 : REM NUL not allowed in encrypted password

P% = LEN(pwd$)
U% = LEN(user$)
D% = LEN(db$)
L% = 40 + U% + P% + D% + LEN(method$)

DIM command&(L%-1)
command&(0) = L% - 4  : REM Length
command&(3) = 1       : REM Sequence ID
command&(4) = 8 + &80 : REM CLIENT_CONNECT_WITH_DB | CLIENT_LOCAL FILES
command&(5) = 2       : REM CLIENT_PROTOCOL_41
command&(6) = 2 + 8   : REM CLIENT_MULTI_RESULTS | CLIENT_PLUGIN_AUTH
command&(7) = 0       : REM MS byte of capabilities
command&(12) = 33     : REM Character set
$$^command&(36) = user$
REM command&(37 + U%) = P%
$$^command&(37 + U%) = pwd$
$$^command&(38 + U% + P%) = db$
$$^command&(39 + U% + P% + D%) = method$

REM Send authentication packet;
R% = FN_writesocket(S%, ^command&(0), L%)
IF R% < L% THEN = -11

REM Read response:
response&() = 0
R% = FN_sqlPacket(S%, 2, response&())
IF R% < 0 THEN = R%

REM Server requests authentication method change:
IF response&(0) = 254 THEN
IF $$^response&(1) <> method$ THEN = -14

L% = 24
command&(0) = LEN(pwd$) : REM Length
command&(3) = 3 : REM Sequence ID
$$^command&(4) = pwd$

REM Send authentication packet again;
R% = FN_writesocket(S%, ^command&(0), L%)
IF R% < L% THEN = -11

REM Read response:
R% = FN_sqlPacket(S%, 4, response&())
IF R% < 0 THEN = R%
ENDIF

IF response&(0) = 255 PROC_sqlError($$^response&(9)) : = -13
IF response&(0) <> 0  THEN = -15

= FALSE

REM Query database and return the number of columns (if any), or negative on error:
REM Previous errors listed plus -16: Couldn't open file, -17: Incomplete upload
DEF FN_sqlQuery(S%, query$)
LOCAL R%, L%, p%%, command&(), response&()
L% = LEN(query$) + 5
DIM response&(255), command&(L%) : REM n.b. room for terminating NUL

command&(0) = (L% - 4) MOD 256
command&(1) = (L% - 4) DIV 256
command&(3) = 0 : REM Sequence ID
command&(4) = 3 : REM COM_QUERY
$$^command&(5) = query$

REM Send COM_QUERY;
R% = FN_writesocket(S%, ^command&(0), L%)
IF R% < L% THEN = -11

REM Read COM_QUERY response:
R% = FN_sqlPacket(S%, 1, response&())
IF R% < 0 THEN = R%
IF response&(0) = &FF PROC_sqlError($$^response&(9)) : = -13
IF response&(0) = &FB THEN = FN_sqlUpload(S%, response&())

p%% = ^response&(0)
= FN_lenenc%(p%%)

REM Read column names and widths, returns the number of columns or negative on error:
DEF FN_sqlColumns(S%, name$(), wide%())
LOCAL I%, R%, p%%, s$, payload&() : DIM payload&(255)
REPEAT
R% = FN_sqlPacket(S%, I% + 2, payload&())
IF R% < 0 THEN = R%
IF payload&(0) = &FE EXIT REPEAT
I% += 1
p%% = ^payload&(0)
s$ = FN_lenenc$(p%%) : REM catalog
s$ = FN_lenenc$(p%%) : REM schema
s$ = FN_lenenc$(p%%) : REM table
s$ = FN_lenenc$(p%%) : REM org_table
s$ = FN_lenenc$(p%%) : REM name
name$(I%) = s$
s$ = FN_lenenc$(p%%) : REM org_name
wide%(I%) = p%%!3
UNTIL FALSE
IF FN_sqlSeqid(I%)
= I%

REM Read (some of) the database, returns the number of rows or negative on error:
DEF FN_sqlFetch(S%, db2d$())
LOCAL C%, I%, L%, R%, p%%, payload&()
DIM payload&(1024)
I% = FN_sqlSeqid(0)
REPEAT
L% = FN_sqlPacket(S%, I% + 3, payload&())
IF L% < 0 THEN = L%
IF payload&(0) = &FE EXIT REPEAT
I% += 1
IF R% < DIM(db2d$(),1) THEN
R% += 1
C% = 0
p%% = ^payload&(0)
REPEAT
IF C% >= DIM(db2d$(),2) EXIT REPEAT
IF p%% >= ^payload&(0) + L% EXIT REPEAT
C% += 1
IF ?p%% = &FB p%% += 1 : db2d$(R%,C%) = "" ELSE db2d$(R%,C%) = FN_lenenc$(p%%)
UNTIL FALSE
ENDIF
UNTIL FALSE
= R%

REM Upload a file, returns zero if successful or negative on error:
DEF FN_sqlUpload(S%, response&())
LOCAL a$, A%, E%, F%, I%, L%, p%%
F% = OPENIN($$^response&(1)) : I% = 2
IF F% THEN
L% = EXT#F%
REPEAT
IF L% <= &10000 A% = L% ELSE A% = &10000
a$ = "    " + GET$#F% BY A%
p%% = PTR(a$) : !p%% = A% : p%%?3 = I%
E% = FN_writesocket(S%, p%%, LEN(a$))
L% -= A% : I% += 1
UNTIL L% = 0 OR E% <> LEN(a$)
ENDIF
p%% = PTR(a$) : !p%% = 0 : p%%?3 = I%
E% = FN_writesocket(S%, p%%, 4)
IF F% = 0 THEN = -16
CLOSE #F%
IF L% <> 0 THEN = -17
IF FN_sqlPacket(S%, I% + 1, response&())
= 0

REM Read a single packet, returns number of bytes read or negative on error:
REM -1: header read fail, -2: header read timeout, -3 unexpected sequence id
REM -4: payload too large, -5: payload read fail, -6: payload read timeout
DEF FN_sqlPacket(S%, seqid&, packet&())
LOCAL L%, N%, R%, T%
REM Read header:
T% = TIME + 500
REPEAT
R% = FN_readsocket(S%, ^packet&(0) + N%, 4 - N%)
IF R% > 0 N% += R%
UNTIL N% >= 4 OR R% < 0 OR TIME > T%
IF R% < 0 THEN = -1
IF N% <> 4 THEN = -2
L% = packet&(0) OR packet&(1) << 8 OR packet&(2) << 16
IF seqid& <> packet&(3) THEN = -3
IF L% > DIM(packet&(),1) + 1 THEN = -4
REM Read payload:
N% = 0
T% = TIME + 200
REPEAT
R% = FN_readsocket(S%, ^packet&(0) + N%, L% - N%)
IF R% > 0 N% += R% : T% = TIME + 200
UNTIL N% >= L% OR R% < 0 OR TIME > T%
IF R% < 0 THEN = -5
IF N% <> L% THEN = -6
= N%

REM Evaluate a length-encoded integer and update pointer:
DEF FN_lenenc%(RETURN p%%) : LOCAL N%
IF ?p%%<&FB N%=?p%% : p%% += 1 : = N%
IF ?p%%=&FC N%=p%%?1 OR p%%?2<<8 : p%% += 3 : = N%
IF ?p%%=&FD N%=p%%?1 OR p%%?2<<8 OR p%%?3<<16 : p%% += 4 : = N%
IF ?p%%=&FE N%=p%%?1 OR p%%?2<<8 OR p%%?3<<16 OR p%%?4<<24 : p%% += 9 : = N%
ERROR 98, "Invalid length-encoded number: " + STR$~?p%%

REM Evaluate a length-encoded string and update pointer:
DEF FN_lenenc$(RETURN p%%) : LOCAL I%, N%, s$
N% = FN_lenenc%(p%%) : IF N% = 0 THEN = ""
s$ = STRING$(N%, CHR$0)
FOR I% = 1 TO N% : MID$(s$,I%,1) = CHR$?p%% : p%% += 1 : NEXT
= s$

REM Set or get error message:
DEF PROC_sqlError(m$) : IF m$ = "" ENDPROC
DEF FN_sqlError : LOCAL m$
PRIVATE e$
IF m$ <> "" e$ = m$  : ENDPROC
= e$

REM Private storage for sequence ID:
DEF FN_sqlSeqid(L%)
PRIVATE P%
SWAP L%,P%
= L%

REM Calculate SHA-1 hash:
DEF FN_sha1(msg$)
LOCAL A%,B%,C%,D%,E%,F%,G%,I%,K%,L%,P%,Q%,R%,S%,T%,Z%,w%()
DIM w%(79)

REM Initialize variables:
P% = &67452301
Q% = &EFCDAB89
R% = &98BADCFE
S% = &10325476
T% = &C3D2E1F0

L% = LEN(msg$)*8

REM Pre-processing:
REM append the bit '1' to the message:
msg$ += CHR$&80

REM append k bits '0', where k is the minimum number >= 0 such that
REM the resulting message length (in bits) is congruent to 448 (mod 512)
WHILE (LEN(msg$) MOD 64) <> 56
msg$ += CHR$0
ENDWHILE

REM append length of message (before pre-processing), in bits, as
REM 64-bit big-endian integer
FOR I% = 56 TO 0 STEP -8
msg$ += CHR$(L% >>> I%)
NEXT

REM Process the message in successive 512-bit chunks:
REM break message into 512-bit chunks, for each chunk
REM break chunk into sixteen 32-bit big-endian words w[i], 0 <= i <= 15

FOR G% = 0 TO LEN(msg$) DIV 64 - 1

FOR I% = 0 TO 15
w%(I%) = !(PTR(msg$) + 64*G% + 4*I%)
SWAP ?(^w%(I%)+0),?(^w%(I%)+3)
SWAP ?(^w%(I%)+1),?(^w%(I%)+2)
NEXT I%

REM Extend the sixteen 32-bit words into eighty 32-bit words:
FOR I% = 16 TO 79
w%(I%) = w%(I%-3) EOR w%(I%-8) EOR w%(I%-14) EOR w%(I%-16)
w%(I%) = (w%(I%) << 1) OR (w%(I%) >>> 31)
NEXT I%

REM Initialize hash value for this chunk:
A% = P%
B% = Q%
C% = R%
D% = S%
E% = T%

REM Main loop:
FOR I% = 0 TO 79
CASE TRUE OF
WHEN 0 <= I% AND I% <= 19
F% = (B% AND C%) OR ((NOT B%) AND D%)
K% = &5A827999
WHEN 20 <= I% AND I% <= 39
F% = B% EOR C% EOR D%
K% = &6ED9EBA1
WHEN 40 <= I% AND I% <= 59
F% = (B% AND C%) OR (B% AND D%) OR (C% AND D%)
K% = &8F1BBCDC
WHEN 60 <= I% AND I% <= 79
F% = B% EOR C% EOR D%
K% = &CA62C1D6
ENDCASE

Z% = FN_32(((A% << 5) OR (A% >>> 27)) + F% + E% + K% + w%(I%))
E% = D%
D% = C%
C% = (B% << 30) OR (B% >>> 2)
B% = A%
A% = Z%

NEXT I%

REM Add this chunk's hash to result so far:
P% = FN_32(P% + A%)
Q% = FN_32(Q% + B%)
R% = FN_32(R% + C%)
S% = FN_32(S% + D%)
T% = FN_32(T% + E%)

NEXT G%

REM Produce the final hash value:
= FN_be(P%) + FN_be(Q%) + FN_be(R%) + FN_be(S%) + FN_be(T%)

DEF FN_sxor(a$,b$)
LOCAL I%
FOR I% = 1 TO LENa$
MID$(a$,I%,1) = CHR$(ASCMID$(a$,I%,1) EOR ASCMID$(b$,I%,1))
NEXT
= a$

DEF FN_be(A%) = CHR$(A%>>24)+CHR$(A%>>16)+CHR$(A%>>8)+CHR$(A%)

DEF FN_32(n%%)=!^n%%


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
REM Routine added for Pico:
REM  FN_gethostip                Return the IP4 address of the device as integer

REM Move HIMEM and BASIC stack down to make room for network heap
DEF PROC__socklib_rsrv(size%)
LOCAL b%%, d%%, s%%
DIM s%% LOCAL TRUE, b%% LOCAL size%-&20, d%% LOCAL TRUE
SYS "memmove", d%%, s%%, HIMEM - s%%
HIMEM = d%% + HIMEM - s%%
ENDPROC

REM Move HIMEM and BASIC stack up recovering space used by network heap
DEF PROC__socklib_free(size%)
LOCAL b%%, d%%, s%%
DIM s%% LOCAL TRUE, b%% LOCAL 7, d%% LOCAL TRUE
b%% = size% + s%% - d%% - &20
s%%!-12 += b%% : d%% = s%% + b%%
SYS "memmove", d%%, s%%, HIMEM - s%%
HIMEM = d%% + HIMEM - s%%
ENDPROC

REM Initialise the BBCSDL Sockets interface
DEF PROC_initsockets(N%)
DEF PROC_initsockets : LOCAL N% : N% = 2
LOCAL chan%, err%, hsize%
SYS "net_heap_size", N% TO hsize%
LOCAL HeapPos{} : DIM HeapPos{bot%,top%}
IF hsize% > 0 THEN
SYS "heap_limits",^HeapPos.bot%,^HeapPos.top%
IF HeapPos.bot% <> HIMEM THEN
DIM vt% TRUE, sp% LOCAL TRUE
IF (sp% - vt%) < hsize% THEN ERROR 0, "No space"
HeapPos.top% = HIMEM
PROC__socklib_rsrv(hsize%)
HeapPos.bot% = HIMEM
ENDIF
ENDIF
SYS "net_init", HeapPos.bot%, HeapPos.top%
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
chan% = OPENOUT("wifi.cfg")
PRINT#chan%, ssid$, pwd$, ccode%
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
LOCAL hsize%, HeapPos{} : DIM HeapPos{bot%,top%}
SYS "net_limits",^HeapPos.bot%,^HeapPos.top%
SYS "net_freeall"
SYS "cyw43_arch_deinit"
IF HeapPos.bot% = HIMEM THEN
hsize% = HeapPos.top% - HeapPos.bot%
SYS "net_term"
PROC__socklib_free(hsize%)
ENDIF
ENDPROC

REM Get the IP address of the local machine
DEF FN_gethostip
LOCAL IPaddress{} : DIM IPaddress{host%}
SYS "net_wifi_get_ipaddr", 0, ^IPaddress.host% TO err%
IF err% <> 0 THEN = 0
= IPaddress.host%

REM Get the name of the local machine
DEF FN_gethostname
LOCAL h%, IPaddress{} : DIM IPaddress{host%}
SYS "net_wifi_get_ipaddr", 0, ^IPaddress.host% TO err%
IF err% <> 0 THEN = ""
SYS "ip4addr_ntoa", ^IPaddress.host% TO h%
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
