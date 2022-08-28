10 REM A very simple chat client to demonstrate the use of TCP on Pico
20 REM v0.1 28-Aug-2022 Memotech-Bill, based upon BBCSDL original
30 REM v1.0, 06-Oct-2017, Richard Russell http://www.rtrussell.co.uk/
40 DIM server{addr%, port%, conn%}
50 ON CLOSE PROCcleanup : QUIT
60 ON ERROR ON ERROR OFF : PROCcleanup : PRINT REPORT$;" Line ";ERL : END
70 PRINT "Connecting to WiFi..."
80 SYS "cyw43_arch_init" TO err%
90 IF err% <> 0 THEN
100 PRINT "Error ";err%;" in initialisation"
110 END
120 ENDIF
130 SYS "cyw43_arch_enable_sta_mode"
140 chan = OPENIN("wifi.cfg")
150 INPUT#chan, ssid$, pwd$
160 CLOSE#chan
170 SYS "cyw43_arch_wifi_connect_timeout_ms", ssid$, pwd$, &400004, 30000 TO err%
180 IF err% <> 0 THEN
190 PRINT "Error ";err%;" connecting to access point"
200 END
210 ENDIF
220 DIM ipaddr{addr%}
230 SYS "net_wifi_get_ipaddr", 0, ^ipaddr.addr% TO err%
240 IF err% <> 0 THEN
250 PRINT "No IP address assigned to connection"
260 END
270 ENDIF
280 SYS "ip4addr_ntoa", ^ipaddr.addr% TO ipname%
290 PRINT "Connected to access point: IP address = ";$$ipname%
300 INPUT "Enter server's IP address (aaa.bbb.ccc.ddd): " server$
310 server.port% = 50343
320 SYS "ip4addr_aton", server$, ^server.addr% TO err%
330 IF err% = 0 THEN
340 PRINT "Error ";err%;" in Server address"
350 END
360 ENDIF
370 port% = 50343
380 port% = 256 * ?^port% + ?(^port% + 1)
390 SYS "net_tcp_connect", ^server.addr%, server.port%, 5000 TO server.conn%
400 IF server.conn% < 0 THEN
410 PRINT "Error ";server.conn%;" connecting to server"
420 END
430 ENDIF
440 PRINT "Connected to server"
450 PRINT "Start typing to chat..."
460 REPEAT
470 key$ = INKEY$(1)
480 IF key$ <> "" THEN
490 COLOUR 4
500 IF ASCkey$ = 27 EXIT REPEAT
510 IF ASCkey$ = &D THEN
520 PRINT
530 chat$ = chat$ + CHR$(13) + CHR$(10)
540 SYS "net_tcp_write", server.conn%, LEN(chat$), PTR(chat$), 5000 TO result%
550 IF result% < 0 EXIT REPEAT
560 chat$ = ""
570 ELSE
580 chat$ = chat$ + key$
590 PRINT CHR$(13);chat$;
600 ENDIF
610 ENDIF
620 result% = FN_readlinesocket(server.conn%, 4, reply$)
630 IF result% > 0 THEN
640 COLOUR 1
650 PRINT
660 PRINT reply$
670 ENDIF
680 UNTIL result% < 0
690 COLOUR 0
700 PRINT "Server closed connection, or error."
710 PROCcleanup
720 END
730 DEF PROCcleanup
740 IF server.conn% > 0 THEN
750 SYS "net_tcp_close", server.conn%
760 SYS "net_freeall"
770 ENDPROC
780 REM Read a line of text to A$ from socket S% with timeout T% centiseconds
790 DEF FN_readlinesocket(S%,T%,RETURN A$)
800 LOCAL buff%,E%,I%
810 PRIVATE B$
820 DIM buff% LOCAL 255
830 REM ON ERROR LOCAL RESTORE LOCAL : ERROR ERR,REPORT$
840 I%=INSTR(B$,CHR$10)
850 IF I%=0 THEN
860 T% += TIME
870 REPEAT
880 SYS "net_tcp_read", S%, 256, buff% TO E%
890 IF E%>0 FOR I%=0 TO E%-1:B$ += CHR$buff%?I% : NEXT
900 I% = INSTR(B$,CHR$10)
910 UNTIL E%<0 OR I%<>0 OR TIME>T%
920 ENDIF
930 IF E%<0 THEN =E%
940 IF I% A$=LEFT$(B$,I%-1) : B$ = MID$(B$,I%+1) ELSE A$=""
950 IF RIGHT$(A$)=CHR$13 A$ = LEFT$(A$)
960 IF ASCB$=13 B$ = MID$(B$,2)
970 =LENA$
