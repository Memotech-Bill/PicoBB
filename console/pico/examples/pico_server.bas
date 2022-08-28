REM A very simple chat server to demonstrate the use of 'socklib'
REM v1.0, 06-Oct-2017, Richard Russell http://www.rtrussell.co.uk/

INSTALL @lib$ + "socklib"
PROC_initsockets

ON CLOSE PROCcleanup : QUIT
ON ERROR ON ERROR OFF : PROCcleanup : PRINT REPORT$ : END

REM. Get local IP address:
myip% = FN_sethost(FN_gethostname)
IF INKEY$(-256) = "W" myip% = !!(myip%!12)

REM. Create a 'listening socket':
socket% = FN_tcplisten("", "50343")
IF socket% = FALSE OR socket% = TRUE PRINT "Couldn't open listening socket" : END

PRINT "Waiting for client connection to ";
PRINT ;myip% AND &FF;".";(myip% >> 8) AND &FF;".";(myip% >> 16) AND &FF;".";myip% >>> 24

REPEAT
WAIT 10
connected% = FN_check_connection(socket%)
UNTIL connected%
socket% = connected%

PRINT "Connected to client "; FN_getpeername(socket%)
PRINT "Start typing to chat..."

REPEAT
key$ = INKEY$(0)
IF key$ <> "" THEN
COLOUR 4
IF ASCkey$ = &D THEN
chat$ = ""
ELSE
exec% = OPENOUT(@tmp$ + "server.tmp")
BPUT #exec%, key$;
CLOSE #exec%
OSCLI "exec """ + @tmp$ + "server.tmp"""
INPUT LINE "" chat$
ENDIF
result% = FN_writelinesocket(socket%, chat$)
IF result% < 0 EXIT REPEAT
ENDIF

result% = FN_readlinesocket(socket%, 4, chat$)
IF result% > 0 THEN
COLOUR 1
PRINT chat$
ENDIF
UNTIL result% < 0

COLOUR 0
PRINT "Client closed connection, or error."
PROCcleanup
END

DEF PROCcleanup
socket% += 0 : IF socket% PROC_closesocket(socket%) : socket% = 0
PROC_exitsockets
ENDPROC
