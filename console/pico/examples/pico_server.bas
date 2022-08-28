10 REM A very simple chat server to demonstrate the use of the use of TCP on Pico
20 REM v0.1 28-Aug-2022 Memotech-Bill, based upon BBCSDL original
30 REM v1.0, 06-Oct-2017, Richard Russell http://www.rtrussell.co.uk/
40 DIM server{addr%, port%, conn%}
50 DIM client{addr%, port%, conn%}
60 ON CLOSE PROCcleanup : QUIT
70 ON ERROR ON ERROR OFF : PROCcleanup : PRINT REPORT$;" Line ";ERL : END
80 PRINT "Connecting to WiFi..."
90 SYS "cyw43_arch_init" TO err%
100 IF err% <> 0 THEN
110 PRINT "Error ";err%;" in initialisation"
120 END
130 ENDIF
140 SYS "cyw43_arch_enable_sta_mode"
150 chan = OPENIN("wifi.cfg")
160 INPUT#chan, ssid$, pwd$
170 CLOSE#chan
180 SYS "cyw43_arch_wifi_connect_timeout_ms", ssid$, pwd$, &400004, 30000 TO err%
190 IF err% <> 0 THEN
200 PRINT "Error ";err%;" connecting to access point"
210 END
220 ENDIF
230 DIM ipaddr{addr%}
240 SYS "net_wifi_get_ipaddr", 0, ^ipaddr.addr% TO err%
250 IF err% <> 0 THEN
260 PRINT "No IP address assigned to connection"
270 END
280 ENDIF
290 SYS "ip4addr_ntoa", ^ipaddr.addr% TO ipname%
300 PRINT "Connected to access point: IP address = ";$$ipname%
310 REM. Create a 'listening socket':
320 server.port% = 50343
330 SYS "net_tcp_listen", ^server.addr%, server.port% TO server.conn%
340 IF server.conn% < 0 PRINT "Couldn't open listening socket" : END
350 PRINT "Waiting for client connection to ";$$ipname%
360 REPEAT
370 WAIT 10
380 SYS "net_tcp_accept", server.conn% TO client.conn%
390 UNTIL client.conn% <> 0
400 IF client.conn% < 0 THEN
410 PRINT "Error ";client.conn%;" accepting connection"
420 END
430 ENDIF
440 SYS "net_tcp_peer", client.conn%, ^client.addr%, ^client.port%
450 SYS "ip4addr_ntoa", ^client.addr% TO ipname%
460 PRINT "Connected to client "; $$ipname%
470 PRINT "Start typing to chat..."
480 REPEAT
490 key$ = INKEY$(1)
500 IF key$ <> "" THEN
510 COLOUR 4
520 IF ASCkey$ = 27 EXIT REPEAT
530 IF ASCkey$ = &D THEN
540 PRINT
550 chat$ = chat$ + CHR$(13) + CHR$(10)
560 SYS "net_tcp_write", client.conn%, LEN(chat$), PTR(chat$), 5000 TO result%
570 IF result% < 0 EXIT REPEAT
580 chat$ = ""
590 ELSE
600 chat$ = chat$ + key$
610 PRINT CHR$(13);chat$;
620 ENDIF
630 ENDIF
640 result% = FN_readlinesocket(client.conn%, 4, reply$)
650 IF result% > 0 THEN
660 COLOUR 1
670 PRINT
680 PRINT reply$
690 ENDIF
700 UNTIL result% < 0
710 COLOUR 0
720 PRINT "Server closed connection, or error."
730 PROCcleanup
740 END
750 DEF PROCcleanup
760 IF client.conn% > 0 THEN
770 SYS "net_tcp_close", client.conn%
780 ENDIF
790 IF server.conn% > 0 THEN
800 SYS "net_tcp_close", server.conn%
810 ENDIF
820 SYS "net_freeall"
830 ENDPROC
840 REM Read a line of text to A$ from socket S% with timeout T% centiseconds
850 DEF FN_readlinesocket(S%,T%,RETURN A$)
860 LOCAL buff%,E%,I%
870 PRIVATE B$
880 DIM buff% LOCAL 255
890 REM ON ERROR LOCAL RESTORE LOCAL : ERROR ERR,REPORT$
900 I%=INSTR(B$,CHR$10)
910 IF I%=0 THEN
920 T% += TIME
930 REPEAT
940 SYS "net_tcp_read", S%, 256, buff% TO E%
950 IF E%>0 FOR I%=0 TO E%-1:B$ += CHR$buff%?I% : NEXT
960 I% = INSTR(B$,CHR$10)
970 UNTIL E%<0 OR I%<>0 OR TIME>T%
980 ENDIF
990 IF E%<0 THEN =E%
1000 IF I% A$=LEFT$(B$,I%-1) : B$ = MID$(B$,I%+1) ELSE A$=""
1010 IF RIGHT$(A$)=CHR$13 A$ = LEFT$(A$)
1020 IF ASCB$=13 B$ = MID$(B$,2)
1030 =LENA$
