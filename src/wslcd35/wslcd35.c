// wslcd35.c - Output to a Waveshare 3.5 inch LCD
#include <stdio.h>
#include <string.h>
#include <hardware/sync.h>
#include "fbufvdu.h"
#include "lfswrap.h"
#include "bbccon.h"
#include "picocli.h"
#include "periodic.h"
#include "LCD_Driver.h"

#include "font_tt.h"

#define WM_LBUTTONDOWN 0x0201

void modechg (char mode);
void error (int iErr, const char *psErr);
void *oshwm (void *addr, int mark);
int putevt (heapptr handler, int msg, int wparam, int lparam);

// VDU variables declared in bbcdata_*.s:
extern int lastx;                       // Graphics cursor x-position (pixels)
extern int lasty;                       // Graphics cursor y-position (pixels)
extern unsigned char cursa;             // Start (top) line of cursor
extern unsigned char cursb;             // Finish (bottom) line of cursor
extern void *vpage;                     // Address of start of BASIC program memory

// Variables defined in fbufvdu.c
extern int xcsr;                        // Text cursor horizontal position
extern int ycsr;                        // Text cursor vertical position
extern int tvt;                         // Top of text viewport
extern int tvb;	                        // Bottom of text viewport
extern int tvl;	                        // Left edge of text viewport
extern int tvr;	                        // Right edge of text viewport
extern int gvt;                         // Top of graphics viewport
extern int gvb;	                        // Bottom of graphics viewport
extern int gvl;	                        // Left edge of graphics viewport
extern int gvr;	                        // Right edge of graphics viewport
extern uint16_t curpal[16];             // Current palette

// Variables defined in fbufctl.c
extern int nCsrHide;                    // Non-zero to hide cursor
extern int csrhgt;                      // Height of cursor
extern int csrtop;                      // Top of cursor in text line

#define PIXEL_FROM_RGB8(r, g, b)    (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

static const uint16_t defpal[16] =
    {
    PIXEL_FROM_RGB8(  0u,   0u,   0u),
    PIXEL_FROM_RGB8(128u,   0u,   0u),
    PIXEL_FROM_RGB8(  0u, 128u,   0u),
    PIXEL_FROM_RGB8(128u, 128u,   0u),
    PIXEL_FROM_RGB8(  0u,   0u, 128u),
    PIXEL_FROM_RGB8(128u,   0u, 128u),
    PIXEL_FROM_RGB8(  0u, 128u, 128u),
    PIXEL_FROM_RGB8(128u, 128u, 128u),
    PIXEL_FROM_RGB8( 64u,  64u,  64u),
    PIXEL_FROM_RGB8(255u,   0u,   0u),
    PIXEL_FROM_RGB8(  0u, 255u,   0u),
    PIXEL_FROM_RGB8(255u, 255u,   0u),
    PIXEL_FROM_RGB8(  0u,   0u, 255u),
    PIXEL_FROM_RGB8(255u,   0u, 255u),
    PIXEL_FROM_RGB8(  0u, 255u, 255u),
    PIXEL_FROM_RGB8(255u, 255u, 255u)
    };

static const uint32_t cpx02[] = { 0x00000000, 0xFFFFFFFF };
static const uint32_t cpx04[] = { 0x00000000, 0x55555555, 0xAAAAAAAA, 0xFFFFFFFF };
static const uint32_t cpx16[] = { 0x00000000, 0x11111111, 0x22222222, 0x33333333,
                                  0x44444444, 0x55555555, 0x66666666, 0x77777777,
                                  0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB,
                                  0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF };

static CLRDEF clrdef[] = {
//   nclr,   cpx, bsh, clrm,     csrmsk
    {   0,  NULL,   0, 0x00,       0x00},
    {   2, cpx02,   0, 0x01, 0x000000FF},
    {   4, cpx04,   1, 0x03, 0x0000FFFF},
    {   8,  NULL,   0, 0x07,       0x00},
    {  16, cpx16,   2, 0x0F, 0xFFFFFFFF}
    };

#define LS  0x10000

static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 320, 256,  40, 32, 111,  0,  8,  40, 0,  8},   // Mode  0 - 10KB
    { 2, 320, 256,  40, 32, 111,  0,  4,  80, 0,  8},   // Mode  1 - 20KB
    { 4, 160, 256,  20, 32, 111,  0,  4,  80, 0,  8},   // Mode  2 - 20KB
    { 1, 320, 225,  40, 25,  15,  0,  8,  40, 1,  9},   // Mode  3 - 10KB
    { 1, 320, 256,  40, 32, 111,  0,  8,  40, 0,  8},   // Mode  4 - 10KB
    { 2, 160, 256,  20, 32, 111,  0,  8,  40, 0,  8},   // Mode  5 - 10KB
    { 1, 320, 225,  40, 25,  15,  0,  8,  40, 1,  9},   // Mode  6 - 10KB
    { 3, 320, 225,  40, 25,  15,  0,  8, 160, 1,TTH},   // Mode  7 - ~1KB - Teletext
    { 1, 320, 480,  40, 30,   0,  0,  8,  40, 0, 16},   // Mode  8 - 19KB
    { 2, 320, 480,  40, 30,   0,  0,  4,  80, 0, 16},   // Mode  9 - 37.5KB
    { 4, 160, 480,  20, 30,   0,  0,  4,  80, 0, 16},   // Mode 10 - 37.5KB
    { 1, 320, 450,  40, 25,  15,  0,  8,  40, 0, 18},   // Mode 11 - 19KB
    { 1, 320, 480,  40, 30,   0,  0,  8,  40, 0, 16},   // Mode 12 - 18.75KB
    { 2, 160, 480,  20, 30,   0,  0,  8,  40, 0, 16},   // Mode 13 - 18.75KB
    { 1, 320, 450,  40, 25,  15,  0,  8,  40, 0, 18},   // Mode 14 - 18.75KB
    { 4, 320, 240,  40, 30,   0,  0,  2, 160, 1,  8},   // Mode 15 - 37.5KB
    { 1, 480, 320,  60, 20,   0, LS,  8,  60, 0, 16},   // Mode 16 - 18.75KB
    { 2, 480, 320,  60, 20,   0, LS,  4, 120, 0, 16},   // Mode 17 - 37.5KB
    { 4, 240, 320,  30, 20,   0, LS,  8,  60, 1, 16},   // Mode 18 - 37.5KB
    { 1, 480, 320,  60, 40,   0, LS,  8,  60, 0,  8},   // Mode 19 - 18.75KB
    { 2, 480, 320,  60, 40,   0, LS,  4, 120, 0,  8},   // Mode 20 - 37.5KB
    { 4, 240, 320,  30, 40,   0, LS,  8,  60, 1,  8},   // Mode 21 - 37.5KB
    };

static CLIFUNC excli = NULL;
static MODE curmode;
static uint8_t *framebuf = NULL;
static bool bInverse = false;
static bool bLScape = false;
static int nrow = 480;
static int scrltop = 0;
static int xscl = 1;
static int yscl = 1;
static CLRDEF *cdef = NULL;

// Control Access to SPI

static int  nSPIInt = 0;

bool SPI_Int_Claim (void)
    {
    if (nSPIInt >= 0)
        {
        ++nSPIInt;
        return true;
        }
    return false;
    }

void SPI_Int_Release (void)
    {
    if (nSPIInt > 0) --nSPIInt;
    }

void SPI_Claim (void)
    {
    uint32_t    intsta = save_and_disable_interrupts ();
    if (nSPIInt <= 0) --nSPIInt;
    restore_interrupts (intsta);
    }

void SPI_Release (void)
    {
    if (nSPIInt < 0) ++nSPIInt;
    }

void gsize (uint32_t *pwth, uint32_t *phgt)
    {
    if (bLScape)
        {
        *pwth = 480;
        *phgt = 320;
        }
    else
        {
        *pwth = 320;
        *phgt = 480;
        }
    }

void wslcd35_out (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2)
    {
    SPI_Claim ();
    bool bWrap = false;
    if (scrltop + (yp1 << curmode.yshf) >= nrow)
        {
        bWrap = true;
        scrltop -= nrow;
        }
    else if (scrltop + (yp2 << curmode.yshf) > nrow)
        {
        wslcd35_out (fbuf, xp1, yp1, xp2, (nrow - scrltop) >> curmode.yshf);
        bWrap = true;
        scrltop -= nrow;
        yp1 = (-scrltop) >> curmode.yshf;
        }
    // bool bShow = nSPIInt <= 0;
	gpio_set_function (LCD_CLK_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MOSI_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MISO_PIN, GPIO_FUNC_SPI);
    // if (bShow) printf ("wslcd35_out (%p, %d, %d, %d, %d)\n", fbuf, xp1, yp1, xp2, yp2);
    if (fbuf != framebuf) return;
    xp1 = xp1 >> (3 - cdef->bitsh);
    xp2 = ((xp2 - 1) >> (3 - cdef->bitsh)) + 1;
    // if (bShow)
    //     {
    //     printf ("ncbt = %d, bitsh = %d\n", curmode.ncbt, cdef->bitsh);
    //     printf ("xp1 = %d, xp2 = %d, xscl = %d, yscl = %d\n", xp1, xp2, xscl, yscl);
    //     printf ("LCD_SetWindow (%d, %d, %d, %d)\n",
    //         xp1 * curmode.nppb, curmode.vmgn + scrltop + (yp1 << curmode.yshf),
    //         xp2 * curmode.nppb, curmode.vmgn + scrltop + ((yp2 + 1) << curmode.yshf) - 1);
    //     }
    LCD_SetWindow (xp1 * curmode.nppb, curmode.vmgn + scrltop + (yp1 << curmode.yshf),
        xp2 * curmode.nppb, curmode.vmgn + scrltop + ((yp2 + 1) << curmode.yshf) - 1);
    fbuf += curmode.nbpl * yp1 + xp1;
    LCD_Write_Init ();
    for (int y = yp1; y < yp2; ++y)
        {
        for (int yr = 0; yr < yscl; ++yr)
            {
            uint8_t *fp = fbuf;
            // if (yp2 == yp1 + 1) printf ("y = %d, yr = %d, fp = %p\n", y, yr, fp);
            for (int x = xp1; x < xp2; ++x)
                {
                uint8_t pix = *fp;
                // if (yp2 == yp1 + 1) printf ("x = %d, pix = 0x%02X\n", x, pix);
                switch (curmode.ncbt)
                    {
                    case 4:
                        LCD_Write_Words (curpal[pix & 0x0F], xscl);
                        LCD_Write_Words (curpal[pix >> 4], xscl);
                        break;
                    case 2:
                        LCD_Write_Words (curpal[pix & 0x03], xscl);
                        LCD_Write_Words (curpal[(pix >> 2) & 0x03], xscl);
                        LCD_Write_Words (curpal[(pix >> 4) & 0x03], xscl);
                        LCD_Write_Words (curpal[pix >> 6], xscl);
                        break;
                    case 1:
                        for (int i = 0; i < 8; ++i)
                            {
                            LCD_Write_Words (curpal[pix & 0x01], xscl);
                            pix >>= 1;
                            }
                        break;
                    }
                ++fp;
                }
            }
        fbuf += curmode.nbpl;
        }
    LCD_Write_Term ();
    if (bWrap) scrltop += nrow;
    SPI_Release ();
    }

void wslcd35_out_int (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2)
    {
    if (SPI_Int_Claim ())
        {
        wslcd35_out (fbuf, xp1, yp1, xp2, yp2);
        SPI_Int_Release ();
        }
    }

void wslcd35_scroll (uint8_t *fbuf, int lt, int lb, bool bUp)
    {
    if ((lt != 0) || (lb != curmode.trow - 1))
        {
        wslcd35_out (fbuf, 0, lt * curmode.thgt, curmode.gcol, (lb + 1) * curmode.thgt);
        }
    else if (bUp)
        {
        scrltop += curmode.thgt << curmode.yshf;
        if ( scrltop >= nrow) scrltop -= nrow;
        SPI_Claim ();
        LCD_Scroll (curmode.vmgn + scrltop);
        SPI_Release ();
        wslcd35_out (fbuf, 0, (curmode.trow - 1) * curmode.thgt, curmode.gcol, curmode.grow);
        }
    else
        {
        scrltop -= curmode.thgt << curmode.yshf;
        if ( scrltop < 0) scrltop += nrow;
        SPI_Claim ();
        LCD_Scroll (curmode.vmgn + scrltop);
        SPI_Release ();
        wslcd35_out (fbuf, 0, 0, curmode.gcol, curmode.thgt);
        }
    }

void bufswap (uint8_t *fbuf)
    {
    if (fbuf == framebuf) wslcd35_out (fbuf, 0, 0, curmode.gcol, curmode.grow);
    }

uint16_t defclr (int clr)
    {
    return defpal[clr];
    }

uint16_t rgbclr (int r, int g, int b)
    {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

int clrrgb (uint16_t clr)
    {
    return ((clr & 0xF800) >> 8) | ((clr & 0x07E0) << 5) | ((clr & 0x001F) << 19);
    }

void genrb (uint16_t *curpal)
    {
    }

bool setmode (int mode, uint8_t **pfbuf, MODE **ppmd, CLRDEF **ppcd)
    {
#if DEBUG & 1
    printf ("setmode (%d)\n", mode);
#endif
    if (( mode >= 0 ) && ( mode < sizeof (modes) / sizeof (modes[0]) ))
        {
        memcpy (&curmode, &modes[mode], sizeof (MODE));
        bool bNewLS = (curmode.hmgn & LS) != 0;
        SPI_Claim ();
        if (bNewLS != bLScape)
            {
            LCD_SetGramScanWay (bNewLS ? D2U_L2R : L2R_U2D);
            bLScape = bNewLS;
            }
        if (curmode.ncbt == 3) xscl = 1;
        else                   xscl = (curmode.ncbt * curmode.nppb) >> 3;
        yscl = 1 << curmode.yshf;
        nrow = curmode.grow << curmode.yshf;
        // printf ("xscl = %d, yscl = %d, nrow = %d, vmgn = %d\n", xscl, yscl, nrow, curmode.vmgn);
        scrltop = 0;
        LCD_ScrollArea (curmode.vmgn, nrow, curmode.vmgn);
        LCD_Scroll (curmode.vmgn + scrltop);
        if (curmode.vmgn > 0)
            {
            LCD_SetAreaColor (0, 0, curmode.gcol * xscl, curmode.vmgn, 0);
            LCD_SetAreaColor (0, nrow + curmode.vmgn, curmode.gcol * xscl, nrow + 2 * curmode.vmgn, 0);
            }
        SPI_Release ();
        framebuf = singlebuf ();
        nCsrHide |= CSR_OFF;
        if (curmode.ncbt == 3)
            {
            csrtop = 0;
            csrhgt = TTH;
            }
        else
            {
            csrtop = curmode.thgt - 1;
            csrhgt = 1;
            }
        *pfbuf = framebuf;
        *ppmd = &curmode;
        cdef = &clrdef[curmode.ncbt];
        *ppcd = cdef;
        if ( curmode.ncbt == 3 ) memcpy (&framebuf[curmode.trow * curmode.tcol], font_tt, sizeof (font_tt));
        return true;
        }
    return false;
    }

bool txtmode (int code, int *pdata1, int *pdata2)
    {
    if ((code & 0x7F) >= sizeof (modes) / sizeof (modes[0])) return false;
    const MODE *pmode = &modes[(code & 0x7F)];
    *pdata2 = (pmode->gcol << 8) | (pmode->grow << 24);
    *pdata1 = (pmode->grow >> 8) | 0x0800 | (pmode->thgt << 16) | (0x01000000 << pmode->ncbt);
    return true;
    }

int bufsize (void)
    {
    int nbyt = curmode.grow * curmode.nbpl;
    if ( curmode.ncbt == 3 ) nbyt += sizeof (font_tt);
    return nbyt;
    }

void dispmode (void)
    {
    nCsrHide = 0;
    showcsr ();
#if DEBUG & 1
    printf ("dispmode: New mode set\n");
#endif
    }

static void wslcd35_ttx (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2, bool bShow)
    {
    // printf ("wslcd35_out7 (%p, %d, %d, %d, %d)\n", fbuf, xp1, yp1, xp2, yp2);
    if (fbuf != framebuf) return;
    SPI_Claim ();
	gpio_set_function (LCD_CLK_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MOSI_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MISO_PIN, GPIO_FUNC_SPI);
    uint16_t blk[TTH][8];
    uint8_t *ttfont = fbuf + curmode.trow * curmode.tcol - 0x20 * TTH;
    bool bDHeight = false;
    bool bDouble = false;
    bool bLower = false;
    for (int yr = 0; yr < yp1; ++yr)
        {
        uint8_t *pch = fbuf + yr * curmode.tcol;
        bDHeight = false;
        for (int xc = 0; xc < curmode.tcol; ++xc)
            {
            if ( *pch & 0x7F == 0x0D )
                {
                bDHeight = true;
                break;
                }
            }
        if (bDHeight) bLower = ! bLower;
        else bLower = false;
        }
    // printf ("LCD_SetWindow (%d, %d, %d, %d)\n", 8 * xscl * xp1, curmode.thgt * yscl * yp1, 8 * xscl * (xp2 + 1), curmode.thgt * yscl * (yp2 + 1));
    for (int yr = yp1; yr <= yp2; ++yr)
        {
        uint8_t *font = ttfont;
        uint8_t *pch = fbuf + yr * curmode.tcol;
        int nfg = 7;
        uint16_t bgnd = curpal[0];
        uint16_t fgnd = curpal[nfg];
        bDHeight = false;
        bDouble = false;
        bool bFlash = false;
        bool bGraph = false;
        bool bCont = true;
        bool bHold = false;
        for (int xc = 0; xc < curmode.tcol; ++xc)
            {
            uint8_t ch = *pch & 0x7F;
            // printf ("yr = %d, xc = %d, ch = 0x%02X\n", yr, xc, ch);
            if ( ch >= 0x20 )
                {
                uint16_t *pblk = &blk[0][0];
                for (int ys = 0; ys < curmode.thgt; ++ys)
                    {
                    int iScan = ys;
                    if (bDouble)
                        {
                        if ( bLower )   iScan = ( ys + TTH ) >> 1;
                        else            iScan = ys >> 1;
                        }
                    uint8_t px = font[TTH * ch + iScan];
                    // printf ("ys = %d, y = %d, px = 0x%02X\n", ys, y, px);
                    if ( px & 0x01 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x02 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x04 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x08 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x10 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x20 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x40 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    if ( px & 0x80 ) *pblk = fgnd;
                    else             *pblk = bgnd;
                    ++pblk;
                    }
                }
            else
                {
                if ( ! bHold )
                    {
                    for (int ys = 0; ys < TTH; ++ys)
                        {
                        for (int i = 0; i < 8; ++i)
                            {
                            blk[ys][i] = bgnd;
                            }
                        }
                    }
                if ((ch >= 0x00) && (ch <= 0x07))
                    {
                    font = ttfont;
                    bGraph = false;
                    nfg = ch & 0x07;
                    fgnd = curpal[nfg];
                    }
                else if (ch == 0x08)
                    {
                    bFlash = true;
                    }
                else if (ch == 0x09)
                    {
                    bFlash = false;
                    }
                else if (ch == 0x0C)
                    {
                    bDouble = false;
                    }
                else if (ch == 0x0D)
                    {
                    bDouble = true;
                    bDHeight = true;
                    }
                else if ((ch >= 0x10) && (ch <= 0x17))
                    {
                    if ( bCont )    font = ttfont + 0x60 * TTH;
                    else            font = ttfont + 0xC0 * TTH;
                    bGraph = true;
                    nfg = ch & 0x07;
                    fgnd = curpal[nfg];
                    }
                else if (ch == 0x19)
                    {
                    bCont = true;
                    if ( bGraph ) font = ttfont + 0x60 * TTH;
                    }
                else if (ch == 0x1A)
                    {
                    bCont = false;
                    if ( bGraph ) font = ttfont + 0xC0 * TTH;
                    }
                else if (ch == 0x1C)
                    {
                    bgnd = curpal[0];
                    }
                else if (ch == 0x1D)
                    {
                    bgnd = curpal[nfg];
                    }
                else if (ch == 0x1E)
                    {
                    bHold = true;
                    }
                else if (ch == 0x1F)
                    {
                    bHold = false;
                    }
                }
            if ((xc >= xp1) && (xc <= xp2))
                {
                int ysr = scrltop + curmode.thgt * yscl * yr;
                if (ysr >= nrow) ysr -= nrow;
                LCD_SetWindow (8 * xscl * xc, curmode.vmgn + ysr, 8 * xscl * (xc + 1), curmode.vmgn + ysr + curmode.thgt * yscl);
                LCD_Write_Init ();
                // printf ("LCD_Write_Words:");
                for (int ys = 0; ys < TTH; ++ys)
                    {
                    if (bInverse && (bFlash ||
                                    ((xc == xcsr) && (yr == ycsr)
                                    && (ys >= csrtop) && (ys < csrtop + csrhgt)
                                    && (nCsrHide == 0))))
                        {
                        LCD_Write_Words (bgnd, 8 * xscl * yscl);
                        // printf (" %0x%02X * %d", bgnd, 8 * xscl);
                        }
                    else if (bShow || bFlash)
                        {
                        for (int y = 0; y < yscl; ++y)
                            {
                            for (int i = 0; i < 8; ++i)
                                {
                                LCD_Write_Words (blk[ys][i], xscl);
                                // printf (" 0x%02X", blk[i]);
                                }
                            }
                        // printf ("\n");
                        }
                    }
                LCD_Write_Term ();
                }
            ++pch;
            }
        if (bDHeight) bLower = ! bLower;
        else bLower = false;
        }
    SPI_Release ();
    }

void wslcd35_out7 (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2)
    {
    wslcd35_ttx (fbuf, xp1, yp1, xp2, yp2, true);
    }

void wslcd35_scroll7 (uint8_t *fbuf, int lt, int lb, bool bUp)
    {
    if ((lt != 0) || (lb != curmode.trow - 1))
        {
        wslcd35_out7 (fbuf, 0, lt, curmode.tcol - 1, lb);
        }
    else if (bUp)
        {
        scrltop += curmode.thgt << curmode.yshf;
        if ( scrltop >= nrow) scrltop -= nrow;
        SPI_Claim ();
        LCD_Scroll (curmode.vmgn + scrltop);
        SPI_Release ();
        wslcd35_out7 (fbuf, 0, curmode.trow - 1, curmode.tcol - 1, curmode.trow - 1);
        }
    else
        {
        scrltop -= curmode.thgt << curmode.yshf;
        if ( scrltop < 0) scrltop += nrow;
        SPI_Claim ();
        LCD_Scroll (curmode.vmgn + scrltop);
        SPI_Release ();
        wslcd35_out7 (fbuf, 0, 0, curmode.tcol - 1, 0);
        }
    }

void mode7flash (void)
    {
    if (SPI_Int_Claim ())
		{
		bInverse = ! bInverse;
		wslcd35_ttx (framebuf, 0, 0, curmode.tcol - 1, curmode.trow - 1, false);
        SPI_Int_Release ();
		}
    }

#if HAVE_MOUSE
#include "LCD_Touch.h"
static int ncal = 0;
static double *pcal = NULL;
static const double defxcal[] = {-0.186488118,  700.0};
static const double defycal[] = { 0.288979808, -112.0};

static inline int nint (double d)
    {
    int n;
    if (d >= 0.0)   n = (int)(d + 0.5);
    else            n = (int)(d - 0.5);
    return n;
    }

// MOUSE x%, y%, b%
void mouse (int *px, int *py, int *pb)
    {
    uint16_t    mx, my;
    int xp = -1;
    int yp = -1;
    int mb = 0;
    SPI_Claim ();
	gpio_set_function (LCD_CLK_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MOSI_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MISO_PIN, GPIO_FUNC_SPI);
    if (TP_Read_TwiceADC (&mx, &my))
        {
        if ((mx != 0) || (my != 4095))
            {
            mb = 1;
            switch (ncal)
                {
                case 2:
                    xp = nint (pcal[0] * mx + pcal[1]);
                    yp = nint (pcal[2] * my + pcal[3]);
                    break;
                case 3:
                    xp = nint (pcal[0] * mx + pcal[1] * my + pcal[2]);
                    yp = nint (pcal[3] * mx + pcal[4] * my + pcal[5]);
                    break;
                case 6:
                    xp = nint (pcal[0] * mx * mx + pcal[1] * mx * my + pcal[2] * my * my
                        + pcal[3] * mx + pcal[4] * my + pcal[5]);
                    yp = nint (pcal[6] * mx * mx + pcal[7] * mx * my + pcal[8] * my * my
                        + pcal[9] * mx + pcal[10] * my + pcal[11]);
                    break;
                default:
                    xp = mx;
                    yp = my;
                    break;
                }
            if ((ncal > 0) && bLScape)
                {
                int tmp = xp;
                xp = yp;
                yp = 640 - xp;
                }
            }
        }
    if (px) *px = xp;
    if (py) *py = yp;
    if (pb) *pb = mb;
    SPI_Release ();
    }

// MOUSE ON [type]
void mouseon (int type)
    {
    }

// MOUSE OFF
void mouseoff (void)
    {
    }

// MOUSE TO x%, y%
void mouseto (int x, int y)
    {
    }
#endif
                      
void mouse_config (int n, const double *xc, const double *yc)
    {
    if (n > ncal)
        {
        if (pcal != NULL) free (pcal);
        pcal = (double *) malloc (2 * n * sizeof (double));
        if (pcal == NULL)
            {
            ncal = 0;
            error (0, NULL);
            }
        }
    memcpy (pcal, xc, n * sizeof (double));
    memcpy (pcal + n, yc, n * sizeof (double));
    ncal = n;
    if (ncal > 0)
        {
        FILE *f = fopen ("/mouse.cfg", "w");
        if (f)
            {
            fwrite (&ncal, sizeof (ncal), 1, f);
            fwrite (pcal, sizeof (double), 2 * ncal, f);
            fclose (f);
            }
        }
    }

static PRD_FUNC pnext = NULL;
static bool bMouseDown = false;

static void mouse_periodic (void)
    {
    if (moutrp)
        {
        // printf ("mouse_periodic\n");
        if (SPI_Int_Claim ())
            {
            // printf ("SPI claimed: moutrp = %p\n", moutrp);
            int mx, my, mb;
            mouse (&mx, &my, &mb);
            // printf ("mouse_periodic: moutrp = %p, mx = %d, my = %d, mb = %d\n", moutrp, mx, my, mb);
            if (mb)
                {
                if (! bMouseDown)
                    {
                    // printf ("putevt: mx = %d, my = %d\n", mx, my);
                    putevt (moutrp, WM_LBUTTONDOWN, 1, my << 16 | (mx & 0xFFFF));
                    bMouseDown = true;
                    }
                }
            else
                {
                bMouseDown = false;
                }
            SPI_Int_Release ();
            }
        }
    if (pnext) pnext ();
    }

void mouse_init (void)
    {
    FILE *f = fopen ("/mouse.cfg", "r");
    if (f)
        {
        int n;
        if (fread (&n, sizeof (n), 1, f) == 1)
            {
            if ((n == 2) || (n == 3) || (n == 6))
                {
                if (n > ncal)
                    {
                    if (pcal != NULL) free (pcal);
                    pcal = (double *) malloc (2 * n * sizeof (double));
                    if (pcal == NULL)
                        {
                        ncal = 0;
                        return;
                        }
                    }
                ncal = n;
                if (fread (pcal, sizeof (double), 2 * ncal, f) != 2 * ncal) ncal = 0;
                }
            }
        fclose (f);
        }
    if (ncal == 0)
        {
        // Default calibration values
        mouse_config (sizeof (defxcal) / sizeof (defxcal[0]), defxcal, defycal);
        }
    pnext = add_periodic (mouse_periodic);
    }

bool brightness (char *cmd)
    {
    if (! strncasecmp (cmd, "brightness", 10))
        {
        cmd += 10;
        while (*cmd == ' ') ++cmd;
        if ((*cmd >= '0') && (*cmd <= '9'))
            {
            int n;
            sscanf (cmd, "%i", &n);
            LCD_SetBackLight (n);
            }
        return true;
        }
    else if (excli)
        {
        return excli (cmd);
        }
    return false;
    }

int setup_vdu (void)
    {
#if DEBUG & 1
    printf ("setup_vdu: Waveshare 3.5 inch LCD: 320x480 " __DATE__ " " __TIME__ "\n");
    sleep_ms(500);
#endif
    SPI_Claim ();
	gpio_set_function (LCD_CLK_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MOSI_PIN, GPIO_FUNC_SPI);
	gpio_set_function (LCD_MISO_PIN, GPIO_FUNC_SPI);
    System_Init ();
    LCD_Init (L2R_U2D, 100);
    SPI_Release ();
    excli = add_cli (brightness);
    setup_fbuf (&curmode);
    memset (singlebuf (), 0, BUF_SIZE);
    modechg (8);
    }
