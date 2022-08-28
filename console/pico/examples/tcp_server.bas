10 PRINT "TCP Server Test"
20 SYS "cyw43_arch_init" TO err%
30 IF err% <> 0 THEN
40 PRINT "Error ";err%;" in initialisation"
50 END
60 ENDIF
70 SYS "cyw43_arch_enable_sta_mode"
80 chan = OPENIN("wifi.cfg")
90 INPUT#chan, ssid$, pwd$
100 CLOSE#chan
110 SYS "cyw43_arch_wifi_connect_timeout_ms", ssid$, pwd$, &400004, 30000 TO err%
120 IF err% <> 0 THEN
130 PRINT "Error ";err%;" connecting to access point"
140 END
150 ENDIF
160 DIM ipaddr{addr%, port%}
170 SYS "net_wifi_get_ipaddr", 0, ^ipaddr.addr% TO err%
171 IF err% <> 0 THEN
172 PRINT "No IP address assigned to connection"
173 END
174 ENDIF
180 SYS "ip4addr_ntoa", ^ipaddr.addr% TO ipname%
190 PRINT "Connected to access point: IP address = ";$$ipname%
200 ipaddr.port% = 4242
205 ipaddr.addr% = 0
210 SYS "net_tcp_listen", ^ipaddr.addr, ipaddr.port% TO listen%
220 IF ( listen% < 0 ) THEN
230 PRINT "Error ";listen%;" opening listening socket"
240 END
250 ENDIF
260 DIM data&(2047)
270 ntest% = 1
280 WHILE TRUE
290 REPEAT
300 SYS "net_tcp_accept", listen% TO conn%
310 IF ( conn% < 0 ) THEN
320 PRINT "Error ";conn%;" accepting connection"
330 SYS "net_tcp_close", listen%
340 END
350 ENDIF
360 UNTIL ( conn% > 0 )
370 SYS "net_tcp_peer", conn%, ^ipaddr.addr%, ^ipaddr.port%
380 ipaddr.port% = 256 * ?^ipaddr.port% + ?(^ipaddr.port% + 1)
390 SYS "ip4addr_ntoa", ^ipaddr.addr% TO ipname%
400 PRINT "Connection from ";$$ipname%;":";ipaddr.port%
410 WHILE TRUE
420 PRINT "Generating test data"
430 t% = 0
440 FOR i% = 0 TO 2047
450 t% = (t% + ntest%) AND 255
460 data&(i%) = t%
470 NEXT
480 SYS "net_tcp_write", conn%, 2048, ^data&(0), 5000 TO err%
490 IF ( err% = -14 ) THEN
500 PRINT "Connection closed"
510 EXIT WHILE
520 ENDIF
530 IF ( err% < 0 ) THEN
540 PRINT "Error ";err%;" sending data"
550 SYS "net_tcp_close", listen%
560 END
570 ENDIF
580 PRINT "Sent ";err%;" bytes"
590 SYS "net_tcp_read", conn%, 2048, ^data&(0), 5000 TO err%
600 IF ( err% = -14 ) THEN
610 PRINT "Connection closed"
620 EXIT WHILE
630 ENDIF
640 IF ( err% < 0 ) THEN
650 PRINT "Error ";err%;" reading data"
660 SYS "net_tcp_close", listen%
670 END
680 ENDIF
690 PRINT "Received ";err%;" bytes"
700 t% = 0
710 err% = 0
720 FOR i% = 0 TO 2047
730 t% = (t% + ntest%) AND 255
740 IF ( data&(i%) <> t% ) THEN
750 err% = err% + 1
760 IF ( err% <= 8 ) THEN PRINT "Mismatch at ";i%;":",t%,data&(i%)
770 ENDIF
780 NEXT
790 PRINT err%;" mismatches"
800 ntest% = ntest% + 1
810 ENDWHILE
820 ENDWHILE
