REM Simple test of IP Address lookup
REM
REM PRINT "HIMEM = ";~HIMEM
INSTALL @lib$ + "socklib"
PROC_initsockets
WHILE TRUE
INPUT "Host Name", H$
IF H$="" THEN EXIT WHILE
H% = FN_sethost(H$)
REM IP Address in Network Byte Order
IF H% <> 0 THEN
PRINT "IP Address = ";H% AND &FF;".";(H%>>8) AND &FF;".";(H%>>16) AND &FF;".";(H%>>24) AND &FF;" (0x";~H%;")"
ELSE
PRINT "IP Address not resolved"
ENDIF
ENDWHILE
PROC_exitsockets
REM PRINT "HIMEM = ";~HIMEM
