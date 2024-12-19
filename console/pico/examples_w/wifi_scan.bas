DIM net{rssi%, chan%, auth%, ssid&(33), mac&(5)}
INSTALL @lib$ + "socklib"
PROC__socklib_initheap
SYS "cyw43_arch_init" TO err%
IF err% <> 0 THEN
PRINT "Error ";err%;" in initialisation"
END
ENDIF
SYS "cyw43_arch_enable_sta_mode"
REPEAT
SYS "net_wifi_scan", net{} TO err%
IF err% = 0 THEN
PRINT net.ssid&()
PRINT "Signal strength: ";net.rssi%
PRINT "Channel: ";net.chan%
PRINT "MAC address: ";
FOR i = 0 TO 4
PRINT RIGHT$("0" + STR$~net.mac&(i), 2);":";
NEXT
PRINT RIGHT$("0" + STR$~net.mac&(5), 2)
PRINT "Authorisation mode: ";net.auth%
PRINT
ENDIF
UNTIL err% <> 0
IF err% = -15 THEN
PRINT "Scan complete"
ELSE
PRINT "Error ";err%
ENDIF
PROC_exitsockets
END