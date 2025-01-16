// picocli.c - Provide PICO specific star commands

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "bbccon.h"
#include "picocli.h"
#ifdef PICO
#ifndef PICO_GUI
#define HAVE_MODEM  1
#include "zmodem.h"
#endif
#endif
#if ( defined(STDIO_USB) || defined(STDIO_UART) )
bool bBBCtl = false;
#endif
#ifdef PICO_GUI
void refresh (const char *p);
#endif
#ifdef PICO_GRAPH
void fbufvdu (int code, int data1, int data2);
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
#undef MAX_PATH
#define MAX_PATH 260
int sdump (FILE *fBmp);
int sload (FILE *fBmp);
#endif

void error (int, const char *);
char *setup (char *dst, const char *src, char *ext, char term, unsigned char *pflag);
#if defined(PICO_GUI) || defined(PICO_GRAPH)
void os_DISPLAY (char *);
#endif
#if ( defined(STDIO_USB) || defined(STDIO_UART) )
void os_OUTPUT (char *);
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
void os_REFRESH (char *);
void os_SCREENSAVE (char *);
#endif
#if HAVE_MODEM
void os_XDOWNLOAD (char *);
void os_XUPLOAD (char *);
void os_YDOWNLOAD (char *);
void os_YUPLOAD (char *);
void os_ZDOWNLOAD (char *);
void os_ZUPLOAD (char *);
#endif

static CLIFUNC excli = NULL;

static char *cmds[] = {
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    "display",
#endif
#if ( defined(STDIO_USB) || defined(STDIO_UART) )
    "output",
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    "refresh", "screensave",
#endif
#if HAVE_MODEM
    "xdownload", "xupload", "ydownload", "yupload", "zdownload", "zupload"
#endif
    };

static void (*os_funcs[])(char *) =
    {
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    os_DISPLAY,     // DISPLAY
#endif
#if ( defined(STDIO_USB) || defined(STDIO_UART) )
    os_OUTPUT,      // OUTPUT
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    os_REFRESH,     // REFRESH
    os_SCREENSAVE,  // SCREENSAVE
#endif
#if HAVE_MODEM
    os_XDOWNLOAD,   // XDOWNLOAD
    os_XUPLOAD,     // XUPLOAD
    os_YDOWNLOAD,   // YDOWNLOAD
    os_YUPLOAD,     // YUPLOAD
    os_ZDOWNLOAD,   // ZDOWNLOAD
    os_ZUPLOAD,     // ZUPLOAD
#endif
    };

#if HAVE_MODEM
void os_XDOWNLOAD (char *p)
    {
    ysend (1, p);
    }

void os_XUPLOAD (char *p)
    {
    yreceive (1, p);
    }

void os_YDOWNLOAD (char *p)
    {
    ysend (2, p);
    }

void os_YUPLOAD (char *p)
    {
    yreceive (2, p);
    }

void os_ZDOWNLOAD (char *p)
    {
    zsend (p);
    }

void os_ZUPLOAD (char *p)
    {
    zreceive (p, NULL);
    }

void os_zmodem (char *p)
    {
    zreceive ("\r", p);
    }
#endif  // HAVE_MODEM

#if ( defined(STDIO_USB) || defined(STDIO_UART) )
void os_OUTPUT (char *p)
    {
    int n = 0;
    sscanf (p, "%i", &n);
    optval = (optval & 0xF0) | (n & 0x0F);
    if ( n & 0x20 ) bBBCtl = (( n & 0x10 ) == 0x10 );
    }
#endif

#if defined(PICO_GUI) || defined(PICO_GRAPH)
void os_DISPLAY (char *p)
    {
    char path[MAX_PATH];
    p = setup (path, p, ".bmp", ' ', NULL);
    FILE *fBmp = fopen (path, "rb");
    if ( fBmp == NULL ) error (214,  "File or path not found");
    int iErr = sload (fBmp);
    fclose (fBmp);
    if ( iErr != 0 ) error (iErr, "Invalid Bitmap");
    }

void os_REFRESH (char *p)
    {
    refresh (p);
    }

void os_SCREENSAVE (char *p)
    {
    char path[MAX_PATH];
    p = setup (path, p, ".bmp", ' ', NULL);
    FILE *fBmp = fopen (path, "wb");
    if ( fBmp == NULL ) error (214,  "File or path not found");
    int iErr = sdump (fBmp);
    fclose (fBmp);
    if ( iErr != 0 ) error (iErr, "Disk fault");
    }
#endif  // defined(PICO_GUI) || defined(PICO_GRAPH)

CLIFUNC add_cli (CLIFUNC new)
    {
    CLIFUNC old = excli;
    excli = new;
    return old;
    }

void org_oscli (char *cmd);

void oscli (char *cmd)
    {
	while (*cmd == ' ') cmd++ ;

	if ((*cmd == 0x0D) || (*cmd == '|'))
		return;

    if (excli)
        {
        if (excli (cmd)) return;
        }

#if HAVE_MODEM
    if ( strncmp (cmd, "*B00", 4) == 0 )
        {
        os_zmodem (cmd);
        return;
        }
#endif
    
	if ( memchr (cmd, 0x0D, 256) != NULL )
        {
        int ncmds = sizeof (cmds) / sizeof (cmds[0]);
        int i1 = -1;
        int i2 = ncmds;
        while ( i2 - i1 > 1 )
            {
            int i3 = ( i1 + i2 ) / 2;
            int n = strlen (cmds[i3]);
            int id = strncasecmp (cmd, cmds[i3], n);
            if ( id == 0 )
                {
                if ( sizeof (os_funcs) / sizeof (os_funcs[0]) != ncmds ) error (255, "Inconsistent arrays in oscli");
                cmd += n;
                if ((*cmd == ' ') || (*cmd == 0x0D) || (*cmd == '|'))
                    {
                    while (*cmd == ' ') cmd++ ;
                    os_funcs[i3] (cmd);
                    return;
                    }
                break;
                }
            else if ( id < 0 )
                {
                i2 = i3;
                }
            else
                {
                i1 = i3;
                }
            }
        }
    org_oscli (cmd);
    }

#ifdef PICO_GRAPH

void org_xeqvdu (int code, int data1, int data2);
bool txtmode (int code, int *pdata1, int *pdata2);

void xeqvdu (int code, int data1, int data2)
    {
    if ((optval & 0x0F) >= 14) fbufvdu (code, data1, data2);
    if ((optval & 0x0F) == 14) return;
    if ((code && 0xFF00) == 0x1600)
        {
        // modechg
        if (! txtmode (code, &data1, &data2)) return;
        code = 0x1700 | (vflags & (CGAFLG + EGAFLG));
        }
    org_xeqvdu (code, data1, data2);
    }
#endif
