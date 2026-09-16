DIM authlist%(5)
authlist%() = 0, &00200002, &00400004, &00400006, &01000004, &01400004
INPUT "SSID", ssid$
INPUT "Password", pwd$
PRINT "Authentication methods:"
PRINT "0: None"
PRINT "1: WPA_TKIP_PSK"
PRINT "2: WPA2_AES_PSK"
PRINT "3: WPA2_MIXED_PSK"
PRINT "4: WPA3_SAE_AES_PSK"
PRINT "5: WPA3_WPA2_AES_PSK"
INPUT "Select method", auth%
IF auth% <= UBOUND(authlist%()) THEN auth% = authlist%(auth%)
LOCAL ccode$, cc1%, cc2%
INPUT "Country Code (2 letters)", ccode$
cc1% = ASC(LEFT$(ccode$, 1))
cc2% = ASC(MID$(ccode$, 2, 1))
IF (cc1% >= 97) AND (cc1% <= 122) THEN cc1% -= 32
IF (cc2% >= 97) AND (cc2% <= 122) THEN cc2% -= 32
ccode% = cc1% + 256 * cc2%
IF ccode% = 19285 THEN ccode% = 16967 : REM UK -> GB
chan% = OPENOUT("wifi.cfg")
PRINT#chan%, ssid$, pwd$, ccode%, auth%
CLOSE#chan%
