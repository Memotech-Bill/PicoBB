*output 14
MODE 9
OFF
CLS
draw% = FALSE
REPEAT
  MOUSE x%, y%, b%
  IF b% THEN
    IF draw% THEN DRAW x%, y% ELSE MOVE x%, y%
    draw% = TRUE
  ELSE
    draw% = FALSE
  ENDIF
  k$ = INKEY$(0)
  IF k$ >= "0" AND k$ <= "3" THEN GCOL(ASC(k$)-ASC("0"))
UNTIL k$ = "Q" OR k$ = "q"
*output 0
