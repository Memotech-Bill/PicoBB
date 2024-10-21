INSTALL @lib$ + "socklib"
PROC_initsockets
WHILE TRUE
INPUT "Host Name", H$
IF H$="" THEN EXIT WHILE
H% = FN_sethost(H$)
PRINT "IP Address = ";(H%>>24) AND &FF;".";(H%>>16) AND &FF;".";(H%>>8) AND &FF;".";H% AND &FF
ENDWHILE
