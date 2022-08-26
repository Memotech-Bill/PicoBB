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
160 PRINT "Connected to access point"
170 DIM server{addr%, port%, conn%}
180 INPUT "Server address", server$, "Server port", server.port%
190 SYS "ip4addr_aton", server$, ^server.addr% TO err%
200 IF err% = 0 THEN
210 PRINT "Error ";err%;" in Server address"
220 END
230 ENDIF
240 port% = 256 * ?^port% + ?(^port + 1)
250 SYS "net_tcp_connect", server.addr%, server.port%, 5000 TO server.conn%
260 IF server.conn% < 0 THEN
270 PRINT "Error ";server.conn%;" connecting to server"
280 END
290 ENDIF
300 PRINT "Connected to server"
310 DIM data&(2047)
320 FOR i% = 1 TO 10
330 SYS "net_tcp_read", server.conn%, 2048, ^data&(0), 10000 TO err%
340 IF err% < 0 THEN
350 PRINT "Error ";err%;" reading data from server"
360 EXIT FOR
370 ENDIF
380 PRINT "Received ";err%;" bytes from server"
390 FOR j% = 0 TO 7
400 PRINT data&(j%),
410 NEXT
420 PRINT
430 SYS "net_tcp_write", server.conn%, err%, ^data&(0), 10000 TO err%
440 IF err% < 0 THEN
450 PRINT "Error ";err%;" writing data to server"
460 EXIT FOR
470 ENDIF
480 PRINT "Sent ";err%;" bytes to server"
490 NEXT
500 SYS "net_tcp_close", server.conn%
510 PRINT "Disconnected from server"
