REM Program to demonstrate the 'mysqllib' library for 'BBC BASIC for SDL 2.0'
REM (should also work with 'BBC BASIC for Windows'). R.T.Russell 18-Aug-2022

SERV$ = "mysql-rfam-public.ebi.ac.uk"
REM SERV$ = "193.62.194.222"
PORT$ = "4497"
USER$ = "rfamro"
PASS$ = ""
DB$   = "Rfam"
TB$   = "author"

MAXROWS = 14
MAXWIDE = 22

ON ERROR IF ERR = 17 CHAIN @lib$ + "../examples/tools/touchide" ELSE               PRINT REPORT$ " at line "; ERL : END

VDU 23,22,800;600;8,17,16,128

INSTALL @lib$ + "mysqllib"
INSTALL @lib$ + "socklib"
PROC_initsockets

REM Connect to MySQL:
PRINT "Connecting to MySQL server '" SERV$ "'... ";
sock% = FN_tcpconnect(SERV$, PORT$)
IF sock% > 0 THEN
PRINT "connected successfully"
ELSE
IF sock% <= 0 PRINT "failed to connect, aborting"
PROC_exitsockets
END
ENDIF
PRINT

REM Connect to database:
PRINT "Connecting to database '" DB$ "'... ";
REPEAT
res% = FN_sqlConnect(sock%, USER$, PASS$, DB$, version$)
IF res% = -12 THEN
PROC_closesocket(sock%)
sock% = FN_tcpconnect(SERV$, PORT$)
ENDIF
UNTIL res% <> -12

IF res% = 0 THEN
PRINT "connected successfully"
ELSE
IF res% PRINT "failed to connect, code = "; res%
IF res% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF
PRINT "MySQL version = "; version$ '

REM Enumerate databases:
PRINT "Fetching database names... ";
res% = FN_sqlQuery(sock%, "SHOW DATABASES")
IF res% < 0 THEN
PRINT "query failed, code = "; res%
IF res% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF

IF res% THEN
DIM dbname$(res%), dbwide%(res%)
ncol% = FN_sqlColumns(sock%, dbname$(), dbwide%())
DIM dbase$(MAXROWS,ncol%)
nrow% = FN_sqlFetch(sock%, dbase$())
PRINT ;nrow% " database(s) enumerated:";
FOR I% = 1 TO nrow%
PRINT " '" dbase$(I%,1) "'";
NEXT
ENDIF
PRINT '

REM Enumerate tables in current database:
PRINT "Fetching table names... ";
res% = FN_sqlQuery(sock%, "SHOW TABLES")
IF res% < 0 THEN
PRINT "query failed, code = "; res%
IF res% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF

IF res% THEN
DIM tbname$(res%), tbwide%(res%)
ncol% = FN_sqlColumns(sock%, tbname$(), tbwide%())
DIM table$(MAXROWS,ncol%)
nrow% = FN_sqlFetch(sock%, table$())
PRINT ;nrow% " table(s) enumerated:";
FOR I% = 1 TO nrow%
PRINT " '" table$(I%,1) "'";
NEXT
ENDIF
PRINT '

REM Fetch table:
PRINT "Querying database '" DB$ "' for table '" TB$ "'..."
ncol% = FN_sqlQuery(sock%, "select * from " + TB$)
IF ncol% < 0 THEN
PRINT "Query failed, code = "; ncol%
IF ncol% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF
PRINT

REM Fetch column names and widths (if any):
PRINT "Fetching column titles... ";
DIM name$(ncol%), wide%(ncol%)
num% = FN_sqlColumns(sock%, name$(), wide%())
IF num% >= 0 THEN
PRINT "number of columns = "; num%
ELSE
PRINT "failed to fetch columns, code = "; res%
IF num% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF

REM Fetch table data:
PRINT "Fetching table data... ";
DIM array$(MAXROWS, ncol%)
nrow% = FN_sqlFetch(sock%, array$())
IF nrow% >= 0 THEN
PRINT "number of rows read = "; nrow%
ELSE
IF nrow% < 0 PRINT "failed to fetch database, code = "; nrow%
IF nrow% = -13 PRINT FN_sqlError
PROC_exitsockets
END
ENDIF
PRINT

REM Print the table heading:
T% = 0
IF num% THEN
FOR C% = 1 TO num%
PRINT TAB(T%) name$(C%);
W% = wide%(C%) : IF W% > MAXWIDE W% = MAXWIDE
T% += W%
NEXT
ENDIF
PRINT

REM Print the returned database:
FOR R% = 1 TO nrow%
T% = 0
FOR C% = 1 TO ncol%
PRINT TAB(T%) array$(R%, C%);
W% = wide%(C%) : IF W% > MAXWIDE W% = MAXWIDE
T% += W%
NEXT
PRINT
NEXT
PRINT

PROC_closesocket(sock%)
PROC_exitsockets

PRINT "Transaction completed successfully";
REPEAT WAIT 4 : UNTIL FALSE
