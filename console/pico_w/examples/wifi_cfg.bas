10 PRINT "Save WiFi configuration to file wifi.cfg"
20 INPUT "SSID", ssid$, "Password", pwd$, "Country Code (2 letters)", ccode$
21 cc1% = ASC(LEFT$(ccode$, 1))
22 cc2% = ASC(MID$(ccode$, 2, 1))
23 IF (cc1% >= 97) AND (cc1% <= 122) THEN cc1% -= 32
24 IF (cc2% >= 97) AND (cc2% <= 122) THEN cc2% -= 32
25 cc1% += 256 * cc2%
30 chan = OPENOUT("wifi.cfg")
40 PRINT#chan, ssid$, pwd$, cc1%
50 CLOSE#chan
