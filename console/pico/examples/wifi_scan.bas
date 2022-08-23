10 DIM ssid&(32)
20 DIM mac&(5)
30 rssi% = 0
40 chan% = 0
50 auth% = 0
60 SYS "cyw43_arch_init" TO err%
70 IF err% <> 0 THEN
80 PRINT "Error ";err%;" in initialisation"
90 END
100 ENDIF
110 SYS "cyw43_arch_enable_sta_mode"
120 REPEAT
130 SYS "net_wifi_scan",  ^ssid&(0), ^rssi%, ^chan%, ^mac&(0), ^auth% TO err%
140 IF err% = 0 THEN
150 PRINT $$^ssid&(0)
160 PRINT "Signal strength: ";rssi%
170 PRINT "Channel: ";chan%
180 PRINT "MAC address: ";
190 FOR i = 0 TO 4
200 PRINT ~mac&(i);":";
210 NEXT
220 PRINT ~mac&(5)
230 PRINT "Authorisation mode: ";auth%
240 PRINT
250 ENDIF
260 UNTIL err% <> 0
270 IF err% = -1 THEN
280 PRINT "Scan complete"
290 ELSE
300 PRINT "Error ";err%
310 ENDIF
320 END
