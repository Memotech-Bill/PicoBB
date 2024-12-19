10 REM An implementation of Trivial File Transfer Protocol
20 PRINT "Trivial File Transfer Protocol Server"
30 DIM buffer&(515)
40 DIM reply&(128)
50 serv% = 0
60 sock% = 0
70 chan% = 0
80 ON CLOSE PROC_cleanup : END
90 ON ERROR PROC_cleanup : PRINT REPORT$;" at line ";ERL : END
100 PRINT "Establishing network connection ..."
110 INSTALL @lib$+"socklib"
120 PROC_initsockets
130 serv% = FN_udpsocket("", "69")
140 IF serv% < 0 THEN PRINT "Error ";serv%;" opening socket" : END
150 PRINT "Waiting for connection on ";FN_gethostname
160 REPEAT
170 E% = FN_recvfromsocket(serv%, ^buffer&(0), 516, claddr%, clport%)
180 IF E% > 2 THEN
190 PRINT "Connection from ";FN_ipname$(claddr%)
200 CASE buffer&(1) OF
210 WHEN 1: E% = FN_tread
220 WHEN 2: E% = FN_twrite
230 ENDCASE
240 ENDIF
250 UNTIL E% < 0
260 PRINT "Connection closed", E%
270 PROC_cleanup
280 END
290 DEF FN_tread
300 sock% = FN_udpsocket("", "1069")
310 IF sock% < 0 THEN = sock%
320 file$ = $$^buffer&(2)
330 PRINT "Read file ";file$
340 chan% = OPENIN (file$)
350 IF chan% THEN
360 PRINT "Transmitting ";
370 nbyte% = 0
380 buffer&(1) = 4
390 buffer&(2) = 0
400 buffer&(3) = 0
410 E% = FN_sendtosocket(sock%, ^buffer&(0), 4, claddr%, clport%)
420 IF E% < 0 THEN = FN_error(E%, " sending request acknowledge")
430 block% = 0
440 rblk% = 0
450 buffer&(1) = 3
460 REPEAT
470 PRINT ".";
480 IF block% = rblk% THEN
490 block% += 1
500 buffer&(2) = block% >>> 8
510 buffer&(3) = block% AND &FF
520 FOR I% = 4 TO 515
530 IF EOF#chan% THEN EXIT FOR
540 buffer&(I%) = BGET#chan%
550 NEXT
560 nbyte% += I% - 4
570 ENDIF
580 E% = FN_sendtosocket(sock%, ^buffer&(0), I%, claddr%, clport%)
590 IF E% < 0 THEN = FN_error(E%, " sending block "+STR$(block%))
600 T% = TIME + 1000
610 raddr% = 0
620 rport% = 0
630 REPEAT
640 E% = FN_recvfromsocket(sock%, ^reply&(0), 128, raddr%, rport%)
650 IF E% < 0 THEN = FN_error(E%, " receiving acknowledge for block "+STR$(block%))
660 IF (E% >= 4) AND (raddr% = claddr%) AND (rport% = clport%) THEN
670 IF reply&(1) = 4 THEN rblk% = 256 * reply&(2) + reply&(3) ELSE E% = -17
680 EXIT REPEAT
690 ELSE
700 E% = -3
710 ENDIF
720 UNTIL TIME > T%
730 IF (E% > 0) AND (rblk% <> block%) AND (rblk% <> (block%-1)) THEN E% = -17
740 IF E% < 0 THEN
750 IF E% = -3 THEN PRINT " Timeout" ELSE PRINT " Teminated" : IF reply&(1) = 5 THEN PRINT $$^reply&(4)
760 buffer&(1) = 5
770 buffer&(2) = 0
780 buffer&(3) = 5
790 E% = FN_sendtosocket(sock%, ^buffer&(0), 4, claddr%, clport%)
800 = E%
810 ENDIF
820 UNTIL EOF#chan%
830 PRINT " Completed"
840 PRINT nbyte%;" bytes transferred"
850 CLOSE#chan%
860 chan% = 0
870 ELSE
880 PRINT "File not found"
890 buffer&(1) = 5
900 buffer&(2) = 0
910 buffer&(3) = 1
920 buffer&(4) = 0
930 E% = FN_sendtosocket(sock%, ^buffer&(0), 5, claddr%, clport%)
940 ENDIF
950 PROC_closesocket(sock%)
960 sock% = 0
970 = E%
980 DEF FN_twrite
990 reply&(0) = 0
1000 sock% = FN_udpsocket("", "1069")
1010 IF sock% < 0 THEN = sock%
1020 file$ = $$^buffer&(2)
1030 PRINT "Write file ";file$
1040 chan% = OPENOUT (file$)
1050 IF chan% THEN
1060 PRINT "Receiving ";
1070 buffer&(1) = 4
1080 buffer&(2) = 0
1090 buffer&(3) = 0
1100 E% = FN_sendtosocket(sock%, ^buffer&(0), 4, claddr%, clport%)
1110 IF E% < 0 THEN = FN_error(E%, "sending acknowledge for request")
1120 block% = 0
1130 nbyte% = 0
1140 REPEAT
1150 block% += 1
1160 T% = TIME + 1000
1170 raddr% = 0
1180 raddr% = 0
1190 REPEAT
1200 E% = FN_recvfromsocket(sock%, ^buffer&(0), 516, raddr%, rport%)
1210 IF E% < 0 THEN = FN_error(E%, " receiving block "+STR$(block%))
1220 IF (E% >= 4) AND (raddr% = claddr%) AND (rport% = clport%) THEN
1230 IF buffer&(1) = 3 THEN rblk% = 256 * buffer&(2) + buffer&(3) ELSE E% = -17
1240 EXIT REPEAT
1250 ENDIF
1260 E% = -3
1270 UNTIL TIME > T%
1280 N% = E%
1290 IF E% >= 4 THEN
1300 PRINT ".";
1310 IF rblk% = block% THEN
1320 FOR I% = 4 TO E% - 1
1330 BPUT#chan%, buffer&(I%)
1340 NEXT
1350 nbyte% += E% - 4
1360 ELSE
1370 IF rblk% <> block% - 1 THEN = FN_error (-17, " unexpected block "+STR$(rblk%))
1380 block% -= 1
1390 ENDIF
1400 reply&(1) = 4
1410 reply&(2) = buffer&(2)
1420 reply&(3) = buffer&(3)
1430 ELSE
1440 IF E% = -3 THEN PRINT " Timeout" ELSE PRINT " Teminated" : IF buffer&(1) = 5 THEN PRINT $$^buffer&(4)
1450 reply&(1) = 5
1460 reply&(2) = 0
1470 reply&(3) = 5
1480 ENDIF
1490 E% = FN_sendtosocket(sock%, ^reply&(0), 4, claddr%, clport%)
1500 IF E% < 0 THEN = FN_error(E%, " sending acknowledge for block "+STR$(block%))
1510 UNTIL N% < 516
1520 CLOSE#chan%
1530 chan% = 0
1540 PRINT " Completed"
1550 PRINT nbyte%;" bytes transferred"
1560 ELSE
1570 PRINT "File not found"
1580 buffer&(1) = 5
1590 buffer&(2) = 0
1600 buffer&(3) = 1
1610 buffer&(4) = 0
1620 E% = FN_sendtosocket(sock%, ^buffer&(0), 5, claddr%, clport%)
1630 ENDIF
1640 PROC_closesocket(sock%)
1650 sock% = 0
1660 = E%
1670 DEF FN_error(E%, msg$)
1680 PRINT " Failed"
1690 PRINT "Error ";E%;msg$
1700 IF sock% <> 0 THEN PROC_closesocket(sock%) : sock% = 0
1710 IF chan% <> 0 THEN CLOSE#chan% : chan% = 0
1720 = E%
1730 DEF FN_ipname$(addr%)
1740 LOCAL name%, ip{} : DIM ip{addr%}
1750 ip.addr% = addr%
1760 SYS "ip4addr_ntoa", ^ip.addr% TO name%
1770 = $$name%
1780 DEF PROC_cleanup
1790 IF chan% <> 0 THEN CLOSE#chan%
1800 IF sock% <> 0 THEN PROC_closesocket(sock%)
1810 IF serv% <> 0 THEN PROC_closesocket(serv%)
1820 PROC_exitsockets
1830 END
1840 ENDPROC
