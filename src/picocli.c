// picocli.c - Provide PICO specific star commands

#include <stdio.h>
#include <string.h>
#include <stdint.h>
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
#ifdef PICO_GRAPH
void graphvdu (int code, int data1, int data2);
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
void refresh (const char *p);
#undef MAX_PATH
#define MAX_PATH 260
int sdump (FILE *fBmp);
int sload (FILE *fBmp);
#endif

void error (int, const char *);
char *setup (char *dst, const char *src, char *ext, char term, unsigned char *pflag);
extern int vpage;

void os_LINENO (const char *);
#if defined(PICO_GUI) || defined(PICO_GRAPH)
void os_DISPLAY (const char *);
#endif
#if ( defined(STDIO_USB) || defined(STDIO_UART) )
void os_OUTPUT (const char *);
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
void os_REFRESH (const char *);
void os_SCREENSAVE (const char *);
#endif
#if HAVE_MODEM
void os_XDOWNLOAD (const char *);
void os_XUPLOAD (const char *);
void os_YDOWNLOAD (const char *);
void os_YUPLOAD (const char *);
void os_ZDOWNLOAD (const char *);
void os_ZUPLOAD (const char *);
#endif

static CLIFUNC excli = NULL;

// Bisection search is used, list must be in alphabetical order
static char *cmds[] = {
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    "display",
#endif
    "lineno",
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

// Order must match that of cmds[]
static void (*os_funcs[])(const char *) =
    {
#if defined(PICO_GUI) || defined(PICO_GRAPH)
    os_DISPLAY,     // DISPLAY
#endif
    os_LINENO,
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

void os_LINENO (const char *p)
    {
    uint16_t n1 = 10;
    uint16_t ns = 10;
    if ((*p >= '0') && (*p <= '9'))
        {
        n1 = 0;
        while ((*p >= '0') && (*p <= '9'))
            {
            n1 = 10 * n1 + *p - '0';
            ++p;
            }
        }
    if (n1 == 0)
        {
        ns = 0;
        }
    else
        {
        while (*p == ' ') ++p;
        if ((*p >= '0') && (*p <= '9'))
            {
            ns = 0;
            while ((*p >= '0') && (*p <= '9'))
                {
                ns = 10 * ns + *p - '0';
                ++p;
                }
            if (ns == 0) ns = 10;
            }
        }
    unsigned char *prg = (unsigned char *)vpage;
    while (*prg != 0)
        {
        prg[1] = n1 & 0xFF;
        prg[2] = n1 >> 8;
        n1 += ns;
        prg += *prg;
        }
    }

#if HAVE_MODEM
void os_XDOWNLOAD (const char *p)
    {
    ysend (1, p);
    }

void os_XUPLOAD (const char *p)
    {
    yreceive (1, p);
    }

void os_YDOWNLOAD (const char *p)
    {
    ysend (2, p);
    }

void os_YUPLOAD (const char *p)
    {
    yreceive (2, p);
    }

void os_ZDOWNLOAD (const char *p)
    {
    zsend (p);
    }

void os_ZUPLOAD (const char *p)
    {
    zreceive (p, NULL);
    }

void os_zmodem (const char *p)
    {
    zreceive ("\r", p);
    }
#endif  // HAVE_MODEM

#if ( defined(STDIO_USB) || defined(STDIO_UART) )
void os_OUTPUT (const char *p)
    {
    int n = 0;
    sscanf (p, "%i", &n);
    optval = (optval & 0xF0) | (n & 0x0F);
    if ( n & 0x20 ) bBBCtl = (( n & 0x10 ) == 0x10 );
    }
#endif

#if defined(PICO_GUI) || defined(PICO_GRAPH)
void os_DISPLAY (const char *p)
    {
    char path[MAX_PATH];
    p = setup (path, p, ".bmp", ' ', NULL);
    FILE *fBmp = fopen (path, "rb");
    if ( fBmp == NULL ) error (214,  "File or path not found");
    int iErr = sload (fBmp);
    fclose (fBmp);
    if ( iErr != 0 ) error (iErr, "Invalid Bitmap");
    }

void os_REFRESH (const char *p)
    {
    refresh (p);
    }

void os_SCREENSAVE (const char *p)
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
    if ((optval & 0x0F) >= 14) graphvdu (code, data1, data2);
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
