10 PRINT "TCP Client Test"
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
160 DIM ipaddr{addr%}
170 SYS "net_wifi_get_ipaddr", 0, ^ipaddr.addr% TO err%
171 IF err% <> 0 THEN
172 PRINT "No IP address assigned to connection"
173 END
174 ENDIF
180 SYS "ip4addr_ntoa", ^ipaddr.addr% TO ipname%
190 PRINT "Connected to access point: IP address = ";$$ipname%
200 DIM server{addr%, port%, conn%}
210 INPUT "Server address", server$
220 server.port% = 4242
230 SYS "ip4addr_aton", server$, ^server.addr% TO err%
240 IF err% = 0 THEN
250 PRINT "Error ";err%;" in Server address"
260 END
270 ENDIF
280 port% = 256 * ?^port% + ?(^port% + 1)
290 SYS "net_tcp_connect", ^server.addr%, server.port%, 5000 TO server.conn%
300 IF server.conn% < 0 THEN
310 PRINT "Error ";server.conn%;" connecting to server"
320 END
330 ENDIF
340 PRINT "Connected to server"
350 DIM data&(2047)
360 FOR i% = 1 TO 10
370 PRINT "Itteration ";i%
380 SYS "net_tcp_read", server.conn%, 2048, ^data&(0), 10000 TO err%
390 IF err% < 0 THEN
400 PRINT "Error ";err%;" reading data from server"
410 EXIT FOR
420 ENDIF
430 PRINT "Received ";err%;" bytes from server"
440 FOR j% = 0 TO 7
450 PRINT data&(j%),
460 NEXT
470 PRINT
480 SYS "net_tcp_write", server.conn%, err%, ^data&(0), 10000 TO err%
490 IF err% < 0 THEN
500 PRINT "Error ";err%;" writing data to server"
510 EXIT FOR
520 ENDIF
530 PRINT "Sent ";err%;" bytes to server"
540 NEXT
550 SYS "net_tcp_close", server.conn%
560 PRINT "Disconnected from server"
