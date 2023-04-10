      REM Program to test uploading a file to a Raspberry Pi Pico
      REM Richard Russell, http://www.rtrussell.co.uk 30-Mar-2023
      REM Revised Memotech-Bill 10-Apr-2023

      INPUT "File name to upload", file$
      fi% = OPENIN(file$)
      IF fi% = 0 THEN
        PRINT "Failed to open BASIC file: "; file$
        END
      ENDIF
      PROC_runpico(file$, fi%)
      END

      DEF PROC_runpico(file$, i%)
      LOCAL cf%, sd%, fin%, ch%, res%, key%, num%, tty%, fd%, f%%, devname$, txt$, cto{}, dcb{}
      cf% = OPENIN(@usr$ + "pico.cfg")
      IF cf% > 0 THEN
        INPUT#cf%, devname$
        CLOSE#cf%
      ELSE
        INPUT "Pico serial port (e.g. COM3: or /dev/ttyACM0)", devname$
        cf% = OPENOUT(@usr$ + "pico.cfg")
        PRINT#cf%, devname$
        CLOSE#cf%
      ENDIF

      PRINT "Resetting USB connected Pico"
      CASE @platform% AND &F OF
        WHEN 0:
          OSCLI "mode " + devname$ + " baud=2400" : REM Reset Pico
          WAIT 100
        WHEN 2:
          OSCLI "stty -f " + devname$ + " 2400"   : REM Reset Pico
          WAIT 100
          OSCLI "stty -f " + devname$ + " 115200 -echo raw"
          tty% = INSTR(devname$, "tty")
          devname$ = LEFT$(devname$,tty%-1) + "cu" + MID$(devname$,tty%+3)
        OTHERWISE:
          OSCLI "stty -F " + devname$ + " 2400"   : REM Reset Pico
          WAIT 100
          OSCLI "stty -F " + devname$ + " 115200 -echo raw"
      ENDCASE

      sd% = OPENUP(devname$ + ".")
      IF sd% = 0 THEN
        PRINT "Unable to open Pico on: ";devname$
        ENDPROC
      ENDIF

      CASE @platform% AND &F OF
        WHEN 0:
          CASE TRUE OF
            WHEN @platform% < &1000000:
              SYS "_fileno", @hfile%(sd%) TO fin%
              SYS "_get_osfhandle", fin% TO f%%
            WHEN (@platform% AND &40) <> 0: f%% = ](@hfile%(sd%)+56)
            OTHERWISE                       f%% = !(@hfile%(sd%)+28)
          ENDCASE

          DIM cto{d%(4)}
          SYS "SetCommTimeouts", f%%, cto{} TO res%
          IF res% = 0 PRINT "SetCommTimeouts failed" : CLOSE #sd% : ENDPROC

          SYS "PurgeComm", f%%, &F TO res%
          IF res% = 0 PRINT "PurgeComm failed" : CLOSE #sd% : ENDPROC

          DIM dcb{d%(6)}
          SYS "BuildCommDCBA", "115200,n,8,1", dcb{}
          SYS "SetCommState", f%%, dcb{} TO res%
          IF res% = 0 PRINT "SetCommState failed" : CLOSE #sd% : ENDPROC

        WHEN 2:
          REM IF @platform% >= &1000000 f%% = ](@hfile%(sd%)+56) ELSE f%% = @hfile%(sd%)
          REM SYS "fileno", f%% TO fd%
          REM *HEX 64
          REM modem% = 2 + 4
          REM SYS "ioctl", fd%, &000000008004746D, ^modem%
      ENDCASE

      CASE FN_yupload(sd%, file$, fi%) OF
        WHEN 0:
          PRINT "Successfully transferred: "; file$
        WHEN 2:
          PRINT "No response from Pico on: "; devname$
          ENDPROC
        WHEN 3:
          PRINT "Failed to complete upload"
          ENDPROC
        WHEN 4:
          PRINT "Failed to select BBC output mode"
          ENDPROC
      ENDCASE

      *ESC OFF
      REPEAT
        key% = INKEY(1)
        IF key% = 1 THEN
           key% = GET
           IF key% = 24 THEN *quit
           IF key% <> 1 THEN BPUT#sd%,1
        ENDIF
        IF key% >0 THEN BPUT#sd%,key%
        PRINT FN_getpico(sd%, 100);
      UNTIL FALSE
      CLOSE#sd%
      ENDPROC

      DEF FN_yupload(sd%, file$, fi%)
      LOCAL i%, nblk%, c$, blk&()
      FOR i% = 1 TO 10
        PRINT "Send ESC"
        BPUT#sd%, &1B
        c$ = FN_getser(sd%, 50)
        PRINT c$
        IF RIGHT$(c$) = CHR$&3E EXIT FOR
        PRINT "Send CR"
        BPUT#sd%, &0D
        c$ = FN_getser(sd%, 100)
        PRINT c$
        IF RIGHT$(c$) = CHR$&3E EXIT FOR
      NEXT
      IF i% > 10 THEN CLOSE #fi% : = 2 : REM No prompt
      FOR i% = 1 TO 10
        PRINT "Send *yupload"
        PRINT#sd%, "*yupload"
        IF FN_gotack(sd%) = &15 THEN EXIT FOR
      NEXT
      IF i% > 10 THEN CLOSE #fi% : = 2 : REM No prompt
      DIM blk&(131)
      REPEAT
        FOR i% = 1 TO 10
          PRINT "Block ";nblk%
          IF nblk% = 0 THEN
            PROC_yblkzero(FN_basename(file$), EXT#fi%, blk&())
          ELSE
            PROC_yblkdata(fi%, nblk%, blk&())
          ENDIF
          IF FN_ysend(sd%, blk&()) EXIT FOR
        NEXT
        IF i% > 10 THEN CLOSE #fi% : = 3
        nblk% += 1
      UNTIL EOF#fi%
      CLOSE#fi%
      PRINT "Send EOT"
      BPUT#sd%,4
      IF FN_gotack(sd%)
      PRINT "Block 0"
      PROC_yblkzero("", 0, blk&())
      IF NOT FN_ysend(sd%, blk&())
      FOR i% = 1 TO 10
        PRINT "Send CR"
        BPUT#sd%, &0D
        c$ = FN_getser(sd%, 50)
        PRINT c$
        IF RIGHT$(c$) = CHR$&3E EXIT FOR
      NEXT
      IF i% > 10 THEN = 3
      FOR i% = 1 TO 10
        PRINT#sd%, "*output 48"
        c$ = FN_getser(sd%, 50)
        PRINT c$
        IF RIGHT$(c$) = CHR$&3E EXIT FOR
      NEXT
      IF i% > 10 THEN = 4
      PRINT#sd%, "CHAIN """ + FN_basename(file$) + """"
      = 0

      DEF PROC_yblkzero(f$, S%, blk&())
      blk&() = 0
      blk&(0) = 1
      blk&(2) = 255
      $$^blk&(3) = f$ + CHR$0 + STR$(S%)
      blk&(131) = SUM(blk&()) AND &FF
      ENDPROC

      DEF PROC_yblkdata(F%, N%, blk&())
      blk&() = 0
      blk&(0) = 1
      blk&(1) = N% AND &FF
      blk&(2) = 255 - blk&(1)
      PTR#F% = 128 * (N% - 1)
      $$^blk&(3) = GET$#F% BY 128
      blk&(131) = SUM(blk&()) AND &FF
      ENDPROC

      DEF FN_ysend(F%, blk&())
      LOCAL I%, P%
      FOR I% = 1 TO 10
        FOR P% = 0 TO 131
          BPUT#F%, blk&(P%)
        NEXT
        IF FN_gotack(F%) = &06 THEN EXIT FOR
      NEXT I%
      = (I% < 11)

      DEF FN_gotack(F%)
      LOCAL A%, I%
      FOR I% = 1 TO 200
        A% = ASCRIGHT$(FN_getser(F%, 1))
        IF A% = &06 OR A% = &15 OR A% = &18 OR A% = &3E THEN EXIT FOR
      NEXT
      = A%

      DEF FN_basename(file$)
      LOCAL I%, N%, c$
      FOR I% = 1 TO LEN(file$)
        c$ = MID$(file$, I%, 1)
        IF c$ = "/" OR c$ = "\" THEN N% = I%
      NEXT
      = MID$(file$, N% + 1)

      DEF FN_getser(F%, O%)
      LOCAL c$, I%, N%, T%
      T% = TIME + O%
      REPEAT
        N% = EXT#F%
        IF N% THEN
          c$ += GET$#F% BY N%
          I% = INSTR(c$, CHR$&1B + "[6n")
          IF I% THEN
            BPUT#F%, CHR$&1B + "[23;79R";
            c$ = LEFT$(c$, I%-1) + MID$(c$, I%+4)
          ENDIF
          T% = TIME + O%
        ELSE
          WAIT 0
        ENDIF
      UNTIL N% = 0 AND TIME >= T%
      REM IF LENc$ PRINT "Received: ";:FOR I% = 1 TO LENc$ : PRINT RIGHT$("0" + STR$~ASCMID$(c$,I%),2) " "; : NEXT : PRINT
      = c$

      DEF FN_getpico(F%, O%)
      LOCAL c$, L%, N%, S%, T%, X%
      T% = TIME + O%
      REPEAT
        N% = EXT#F%
        IF N% THEN
          c$ += GET$#F% BY N%
          N% += L%
          L% += S%
          WHILE L% < N%
            L% += 1
            S% = 0
            CASE X% OF
              WHEN 1 :
                IF ASC(MID$(c$, L%, 1)) = 31 THEN
                  X% = 2
                ELSE
                  X% = 0
                  S% = 8
                ENDIF
              WHEN 2 :
                IF ASC(MID$(c$, L%, 1)) = 0 THEN BPUT#F%, CHR$&1B + "[" + STR$(VPOS) + ";" + STR$(POS) + "R";
                c% = LEFT$(c$, L%-3) + MID$(c$, L%+1)
                X% = 0
              OTHERWISE :
                CASE ASC(MID$(c$, L%, 1)) OF
                  WHEN 1, 17, 22, 27 : S% = 1
                  WHEN 18, 31 : S% = 2
                  WHEN 28, 29 : S% = 4
                  WHEN 19, 25 : S% = 5
                  WHEN 24 : S% = 8
                  WHEN 23 : X% = 1
                ENDCASE
            ENDCASE
            L% += S%
          ENDWHILE
          S% = L% - N%
          IF S% > 0 THEN L% = N% ELSE S% = 0
        ENDIF
      UNTIL ( S% = 0 ) AND ( NOT X% ) AND ( TIME >- T% )
      = c$
