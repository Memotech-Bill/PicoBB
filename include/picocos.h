// picocos.h - Macros to customise bbccos.c for use in PicoBB

#ifndef PICOCOS_H
#define PICOCOS_H

// Work around out of order output when using Pico printf
#define printf(...) fprintf (stdout, __VA_ARGS__)

// Rename oscli so that Pico can add additional star commands

#define oscli   org_oscli

// Remove routines replaced in GUI mode

#if defined(PICO_GUI)
#define modechg null_modechg
#define defchr  null_defchr
#define newline null_newline
#define xeqvdu  null_xeqvdu

#elif defined(PICO_GRAPH)
#define modechg null_modechg
#define xeqvdu  org_xeqvdu
#endif

#endif
