PROC_TPad_Setup
END

DEF PROC_TPad_Setup
*output 14
MODE 9
*output 15
CLS
REM    0123456789012345678901234567890123456789
PRINT "To calibrate the touchpad -"
PRINT "Touch the centre of each target"
PRINT ""
PRINT "Press a key to begin"
IF INKEY(10000)>0 THEN PROC_TPad_Calibrate ELSE CLS
*output 0
ENDPROC

DEF FN_FullTri(a#(), ipiv%(), jpiv%())
 LOCAL m%
 m% = DIM(a#(),1)
 LOCAL i%, j%, k%
 FOR k% = 0 TO m%
   ipiv%(k%) = k%
   jpiv%(k%) = k%
 NEXT
 LOCAL p#, pp#, pr#, ii%, jj%, ip%, jp%
 FOR k% = 0 TO m% - 1
   p# = 0.0
   FOR i% = k% TO m%
     ii% = ipiv%(i%)
     pr# = 0.0
     FOR j% = k% TO m%
       pr# = pr# + ABS(a#(ii%, jpiv%(j%)))
     NEXT
     FOR j% = k% TO m%
       pp# = ABS(a#(ii%, jpiv%(j%))) / pr#
       IF pp# > p# THEN
         p# = pp#
         ip% = i%
         jp% = j%
       ENDIF
     NEXT
   NEXT
   IF p# = 0.0 THEN = FALSE
   ii% = ipiv%(k%)
   jj% = jpiv%(k%)
   ipiv%(k%) = ipiv%(ip%)
   jpiv%(k%) = jpiv%(jp%)
   ipiv%(ip%) = ii%
   jpiv%(jp%) = jj%
   ip% = ipiv%(k%)
   jp% = jpiv%(k%)
   pr# = a#(ip%,jp%)
   pr# = a#(ip%, jp%)
   FOR i% = k% + 1 TO m%
     ii% = ipiv%(i%)
     p# = a#(ii%, jp%) / pr#
     a#(ii%, jp%) = p#
     FOR j% = k% + 1 TO m%
       jj% = jpiv%(j%)
       a#(ii%, jj%) = a#(ii%, jj%) - p# * a#(ip%, jj%)
     NEXT
   NEXT
 NEXT
 = TRUE

DEF PROC_TriSolve(a#(), ipiv%(), jpiv%(), b#(), x#())
  LOCAL m%, i%, j%, ip%, jp%, sum#
  m% = DIM(a#(),1)
  x#(jpiv%(0)) = b#(ipiv%(0))
  FOR i% = 1 TO m%
    ip% = ipiv%(i%)
    sum# = 0.0
    FOR j% = 0 TO i% - 1
      jp% = jpiv%(j%)
      sum# = sum# + a#(ip%, jp%) * x#(jp%)
    NEXT
    x#(jpiv%(i%)) = b#(ip%) - sum#
  NEXT
  ip% = ipiv%(m%)
  jp% = jpiv%(m%)
  x#(jp%) = x#(jp%) / a#(ip%, jp%)
  FOR i% = m% - 1 TO 0 STEP -1
    ip% = ipiv%(i%)
    sum# = 0.0
    FOR j% = i% + 1 TO m%
      jp% = jpiv%(j%)
      sum# = sum# + a#(ip%, jp%) * x#(jp%)
    NEXT
    jp% = jpiv%(i%)
    x#(jp%) = (x#(jp%) - sum#) / a#(ip%, jp%)
  NEXT
ENDPROC

DEF PROC_TPad_Target(x%, y%)
  LINE x%-10, y%-10, x%+10, y%+10
  LINE x%+10, y%-10, x%-10, y%+10
  CIRCLE x%, y%, 14
ENDPROC

DEF PROC_TPad_Sample(RETURN xa%, RETURN ya%)
  LOCAL xp%, yp%, b%, c%
  xa% = 0
  ya% = 0
  c% = 0
  REPEAT
    MOUSE xp%, yp%, b%
  UNTIL b% = 1
  REPEAT
    xa% = xa% + xp%
    ya% = ya% + yp%
    c% = c% + 1
    MOUSE xp%, y%, b%
  UNTIL b% = 0
  xa% = xa% / c%
  ya% = ya% / c%
ENDPROC

DEF PROC_TPad_Calibrate
  LOCAL xt%(), yt%()
  DIM xt%(2), yt%(2)
  LOCAL w#(), xr#(), xf#(), yr#(), yf#()
  DIM w#(5), xr#(5), xf#(5), yr#(5), yf#(5)
  LOCAL c#(), d#()
  DIM c#(5, 5), DIM d#(5,5)
  LOCAL ipiv%(), jpiv%()
  DIM ipiv%(5), jpiv%(5)
  xt%() = 50, 320, 590
  yt%() = 50, 480, 910
  CLS
  *output 14
  SYS "mouse_config", 0, 0, 0
  done% = TRUE
  REPEAT
    c#() = 0
    xr#() = 0
    yr#() = 0
    xs# = 0
    ys# = 0
    *output 14
    FOR i% = 0 TO 2
      FOR j% = 0 TO 2
        GCOL 1
        PROC_TPad_Target(xt%(i%), yt%(j%))
        PROC_TPad_Sample(xa%, ya%)
        GCOL 0
        PROC_TPad_Target(xt%(i%), yt%(j%))
        WAIT 50
        w#(0) = xa% * xa%
        w#(1) = xa% * ya%
        w#(2) = ya% * ya%
        w#(3) = xa%
        w#(4) = ya%
        w#(5) = 1.0
        FOR ii% = 0 TO 5
          FOR jj% = 0 TO 5
            c#(ii%, jj%) = c#(ii%, jj%) + w#(ii%) * w#(jj%)
          NEXT
          xr#(ii%) = xr#(ii%) + w#(ii%) * xt%(i%)
          yr#(ii%) = yr#(ii%) + w#(ii%) * yt%(j%)
        NEXT
        xs# = xs# + xt%(i%) * xt%(i%)
        ys# = ys# + yt%(j%) * yt%(j%)
      NEXT
    NEXT
    d#() = c#()
    IF FN_FullTri(c#(), ipiv%(), jpiv%()) THEN
      PROC_TriSolve(c#(), ipiv%(), jpiv%(), xr#(), xf#())
      PROC_TriSolve(c#(), ipiv%(), jpiv%(), yr#(), yf#())
      *output 0
      FOR i% = 0 TO 5
        PRINT "Row ";i%;
        FOR j% = 0 TO 5
          xs# = xs# + d#(i%, j%) * xf#(i%) * xf#(j%)
          ys# = ys# + d#(i%, j%) * yf#(i%) * yf#(j%)
          PRINT ",";d#(i%,j%);
        NEXT
        xs# = xs# - 2.0 * xr#(i%) * xf#(i%)
        ys# = ys# - 2.0 * yr#(i%) * yf#(i%)
      NEXT
      xs# = xs# / 9.0
      ys# = ys# / 9.0
      IF xs# + ys# < 2500 THEN done% = TRUE
    ENDIF
  UNTIL done%
  *output 0
  SYS "mouse_config", 6, ^xf#(0), ^yf#(0)
ENDPROC
