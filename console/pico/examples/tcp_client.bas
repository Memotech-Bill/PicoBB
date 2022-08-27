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
160 SYS "net_wifi_get_ipaddr", 0 TO ipaddr%
170 SYS "ip4addr_ntoa", ^ipaddr% TO ipname%
180 PRINT "Connected to access point: IP address = ";$$ipname%
190 DIM server{addr%, port%, conn%}
200 INPUT "Server address", server$, "Server port", server.port%
210 SYS "ip4addr_aton", server$, ^server.addr% TO err%
220 IF err% = 0 THEN
230 PRINT "Error ";err%;" in Server address"
240 END
250 ENDIF
260 port% = 256 * ?^port% + ?(^port + 1)
270 SYS "net_tcp_connect", server.addr%, server.port%, 5000 TO server.conn%
280 IF server.conn% < 0 THEN
290 PRINT "Error ";server.conn%;" connecting to server"
300 END
310 ENDIF
320 PRINT "Connected to server"
330 DIM data&(2047)
340 FOR i% = 1 TO 10
350 SYS "net_tcp_read", server.conn%, 2048, ^data&(0), 10000 TO err%
360 IF err% < 0 THEN
370 PRINT "Error ";err%;" reading data from server"
380 EXIT FOR
390 ENDIF
400 PRINT "Received ";err%;" bytes from server"
410 FOR j% = 0 TO 7
420 PRINT data&(j%),
430 NEXT
440 PRINT
450 SYS "net_tcp_write", server.conn%, err%, ^data&(0), 10000 TO err%
460 IF err% < 0 THEN
470 PRINT "Error ";err%;" writing data to server"
480 EXIT FOR
490 ENDIF
500 PRINT "Sent ";err%;" bytes to server"
510 NEXT
520 SYS "net_tcp_close", server.conn%
530 PRINT "Disconnected from server"
