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
370 SYS "net_tcp_connect", ^server.addr%, server.port%, 5000 TO server.conn%
380 IF server.conn% < 0 THEN
390 PRINT "Error ";server.conn%;" connecting to server"
400 END
410 ENDIF
420 PRINT "Connected to server"
430 PRINT "Start typing to chat..."
440 REPEAT
450 key$ = INKEY$(1)
460 IF key$ <> "" THEN
470 COLOUR 4
480 IF ASCkey$ = 27 EXIT REPEAT
490 IF ASCkey$ = &D THEN
500 PRINT
510 chat$ = chat$ + CHR$(13) + CHR$(10)
520 SYS "net_tcp_write", server.conn%, LEN(chat$), PTR(chat$), 5000 TO result%
530 IF result% < 0 EXIT REPEAT
540 chat$ = ""
550 ELSE
560 chat$ = chat$ + key$
570 PRINT CHR$(13);chat$;
580 ENDIF
590 ENDIF
600 result% = FN_readlinesocket(server.conn%, 4, reply$)
610 IF result% > 0 THEN
620 COLOUR 1
630 PRINT
640 PRINT reply$
650 ENDIF
660 UNTIL result% < 0
670 COLOUR 0
680 PRINT "Server closed connection, or error."
690 PROCcleanup
700 END
710 DEF PROCcleanup
720 IF server.conn% > 0 THEN
730 SYS "net_tcp_close", server.conn%
740 ENDIF
750 SYS "net_freeall"
760 ENDPROC
770 REM Read a line of text to A$ from socket S% with timeout T% centiseconds
780 DEF FN_readlinesocket(S%,T%,RETURN A$)
790 LOCAL buff%,E%,I%
800 PRIVATE B$
810 DIM buff% LOCAL 255
820 REM ON ERROR LOCAL RESTORE LOCAL : ERROR ERR,REPORT$
830 I%=INSTR(B$,CHR$10)
840 IF I%=0 THEN
850 T% += TIME
860 REPEAT
870 SYS "net_tcp_read", S%, 256, buff% TO E%
880 IF E%>0 FOR I%=0 TO E%-1:B$ += CHR$buff%?I% : NEXT
890 I% = INSTR(B$,CHR$10)
900 UNTIL E%<0 OR I%<>0 OR TIME>T%
910 ENDIF
920 IF E%<0 THEN =E%
930 IF I% A$=LEFT$(B$,I%-1) : B$ = MID$(B$,I%+1) ELSE A$=""
940 IF RIGHT$(A$)=CHR$13 A$ = LEFT$(A$)
950 IF ASCB$=13 B$ = MID$(B$,2)
960 =LENA$
