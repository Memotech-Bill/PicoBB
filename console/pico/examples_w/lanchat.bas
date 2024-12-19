REM Simple multi-user 'serverless' chat program using UDP sockets
REM Each chat participant must be using a separate client machine
REM The program uses broadcast mode so will work only over a LAN.
REM Minor revisions for running on Pico W.

INSTALL @lib$ + "socklib"

PROC_initsockets : REM. Initialise Windows or SDL_net sockets

ON CLOSE PROCcleanup : QUIT
ON ERROR ON ERROR OFF : PROCcleanup : PRINT REPORT$:END

broadcast% = &FFFFFFFF : REM Broadcast IP address
port$ = "2451" : REM Unofficial use of netchat port
host% = FN_sethost(FN_gethostname)
port% = FN_setport(port$)

REM Enable UTF-8 mode:
VDU 23,22,640;500;8,20,16,128+8

REM Set font and colours:
IF INKEY$(-256) = "W" THEN
*font Consolas,14
ELSE IF @platform% > &2000000 THEN;
OSCLI "font """ + @lib$ + "DejaVuSansMono.ttf"",14"
IF (@platform% AND 7) >= 3 THEN *OSK ON
ENDIF
COLOUR 2,0,128,0
COLOUR 3,128,96,0
COLOUR 6,0,64,255

REM. Create a UDP socket for sending and receiving:
UDP_socket% = FN_udpsocket("",port$)
IF UDP_socket% <= 0 THEN
PRINT "Error: failed to create UDP socket."
PRINT "Is another copy of the program running?"
PRINT "Are you connected to a Local Area Network?"
REPEAT WAIT 2 : UNTIL FALSE
ENDIF

REM allocate buffer for received messages:
buflen% = 512
DIM buffer%% buflen%, hosts%(7), seq&(7)

REM ascertain 'true' local IP address:
REM result% = FN_sendtosocket(UDP_socket%, buffer%%, 1, broadcast%, port%)
REM REPEAT
REM result% = FN_recvfromsocket(UDP_socket%, buffer%%, buflen%, host%, discard%)
REM UNTIL result% = 1
REM hosts%(0) = broadcast%
REM Pico does not receive its own broadcasts
host% = FN_gethostip
hosts%(0) = broadcast%

PRINT "Start typing then hit Return/Enter to chat..."
REPEAT
key$ = INKEY$(2)
IF key$ <> "" THEN
COLOUR 0
PRINT FNip(host%) ":" TAB(16);
IF ASCkey$ = &D THEN
chat$ = ""
PRINT
ELSE
exec% = OPENOUT(@tmp$ + "chat.tmp")
BPUT #exec%, key$;
CLOSE #exec%
OSCLI "exec """ + @tmp$ + "chat.tmp"""
COLOUR 1
INPUT LINE "" chat$
ENDIF
seq& += 1
chat$ = CHR$(seq&) + chat$
REM Each message is first broadcast and then sent to each host individually
REM This helps with delivery over WiFi which doesn't handle broadcasts well
FOR repeat% = 0 TO DIM(hosts%(),1)
IF hosts%(repeat%) THEN
result% = FN_sendtosocket(UDP_socket%, PTR(chat$), LEN(chat$), hosts%(repeat%), port%)
IF result% < 0 EXIT REPEAT
WAIT 1
ENDIF
NEXT
IF INKEY$(-256) <> "W" IF (@platform% AND 7) = 4 THEN *OSK ON
ENDIF

result% = FN_recvfromsocket(UDP_socket%, buffer%%, buflen%, from%, discard%)
IF result% > 0 IF from% <> host% THEN
buffer%%?result% = 0
C% = 0
REPEAT
C% += 1
UNTIL hosts%(C%) = from% OR hosts%(C%) = 0 OR C% = DIM(hosts%(),1)
IF hosts%(C%) = 0 hosts%(C%) = from%
IF ?buffer%% <> seq&(C%) THEN
seq&(C%) = ?buffer%%
COLOUR 0
PRINT FNip(from%) ":" TAB(16);
COLOUR C% + 1
PRINT $$(buffer%% + 1)
ENDIF
ENDIF
UNTIL result% < 0

COLOUR 0
PRINT "Sorry, an error occurred (disconnected from network?)"
PROCcleanup
REPEAT WAIT 2: UNTIL FALSE
END

DEF FNip(H%)
= STR$(H% AND &FF)+"."+STR$((H%>>8) AND &FF)+"."+STR$((H%>>16) AND &FF)+"."+STR$(H%>>>24)

DEF PROCcleanup
UDP_socket% += 0 : IF UDP_socket% PROC_closesocket(UDP_socket%)
PROC_exitsockets
ENDPROC

