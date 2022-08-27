10 DIM net{rssi%, chan%, auth%, ssid&(32), mac&(5)}
20 SYS "cyw43_arch_init" TO err%
30 IF err% <> 0 THEN
40 PRINT "Error ";err%;" in initialisation"
50 END
60 ENDIF
70 SYS "cyw43_arch_enable_sta_mode"
80 REPEAT
90 SYS "net_wifi_scan", net{} TO err%
100 IF err% = 0 THEN
110 PRINT net.ssid&()
120 PRINT "Signal strength: ";net.rssi%
130 PRINT "Channel: ";net.chan%
140 PRINT "MAC address: ";
150 FOR i = 0 TO 4
160 PRINT RIGHT$("0" + STR$~net.mac&(i), 2);":";
170 NEXT
180 PRINT RIGHT$("0" + STR$~net.mac&(5), 2)
190 PRINT "Authorisation mode: ";net.auth%
200 PRINT
210 ENDIF
220 UNTIL err% <> 0
230 IF err% = -15 THEN
240 PRINT "Scan complete"
250 ELSE
260 PRINT "Error ";err%
270 ENDIF
280 END