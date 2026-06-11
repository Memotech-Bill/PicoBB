REM Utility routines for PicoCalc

REM Get Battery charge status
DEF FN_GetBattery()
    LOCAL cmd&, dat&
    cmd& = &0B
    SYS "PC_Request", cmd&, dat& TO dat&
    =dat&

REM Get LCD backlight setting (0-255)
DEF FN_GetLCDBacklight()
    LOCAL cmd&, dat&
    cmd& = &05
    SYS "PC_Request", cmd&, dat& TO dat&
    =dat&

REM Get keyboard backlight setting (0-255)
DEF FN_GetKbdBacklight()
    LOCAL cmd&, dat&
    cmd& = &0A
    SYS "PC_Request", cmd&, dat& TO dat&
    =dat&

REM Set LCD backlight brightness (0-255)
DEF PROC_SetLCDBacklight(dat&)
    LOCAL cmd&
    cmd& = &85
    SYS "PC_Request", cmd&, dat&
ENDPROC

REM Set keyboard backlight brightness (0-255)
DEF PROC_SetKbdBacklight(dat&)
    LOCAL cmd&
    cmd& = &8A
    SYS "PC_Request", cmd&, dat&
ENDPROC

REM Read data from the 8Mbit PSRAM
REM addr% = Address in PSRAM
REM len%  = Length of data to read (in bytes)
REM bptr% = Address of BASIC memory to receive data (use ^ operator)
REM Note that the first 153,602 bytes are used by *refresh (894,974 bytes still available)
DEF PROC_PSRAM_Read(addr%, len%, bptr%)
    LOCAL qread%, qwait%, alim%, n%
    qread% = SYS("qspi_qread")
    qwait% = SYS("qspi_wait")
    SYS "qspi_cfg_qread"
    WHILE len%>0
          alim% = (addr% + &400) AND &FFFFFC00
          n% = alim% - addr%
          if n% > len% then n% = len%
          SYS qread%, addr%, n%, bptr%
          SYS qwait%
          addr% = addr% + n%
          bptr% = bptr% + n%
          len% = len% - n%
    ENDWHILE
    SYS "qspi_free"
ENDPROC

REM Write data to the 8Mbit PSRAM
REM addr% = Address in PSRAM
REM len%  = Length of data to write (in bytes)
REM bptr% = Address of BASIC memory containing the data (use ^ operator)
REM Note that the first 153,602 bytes are used by *refresh (894,974 bytes still available)
DEF PROC_PSRAM_Write(addr%, len%, bptr%)
    LOCAL qwrite%, qwait%, alim%, n%
    qwrite% = SYS("qspi_qwrite")
    qwait% = SYS("qspi_wait")
    SYS "qspi_cfg_qwrite"
    WHILE len%>0
          alim% = (addr% + &400) AND &FFFFFC00
          n% = alim% - addr%
          if n% > len% then n% = len%
          SYS qwrite%, addr%, n%, bptr%
          SYS qwait%
          addr% = addr% + n%
          bptr% = bptr% + n%
          len% = len% - n%
    ENDWHILE
    SYS "qspi_free"
ENDPROC
