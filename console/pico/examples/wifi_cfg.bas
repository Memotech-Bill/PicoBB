10 PRINT "Save WiFi configuration to file wifi.cfg"
20 INPUT "SSID", ssid$, "Password", pwd$
30 chan = OPENOUT("wifi.cfg")
40 PRINT#chan, ssid$, pwd$
50 CLOSE#chan
