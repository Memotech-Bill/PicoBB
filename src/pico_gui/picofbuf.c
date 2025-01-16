/*  picofbuf.c - Framebuffer VGA Display for BBC Basic on Pico */

#define USE_INTERP  1
#define HIRES       0   // CAUTION - Enabling HIRES requires extreme Pico overclock

// DEBUG =  0   No diagnostics
//          1   General diagnostics
//          2   Interpolator test
//          4   Double buffering
//          8   Buffer swap
#define DEBUG       0

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/scanvideo_base.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#if USE_INTERP
#include "hardware/interp.h"
#endif
#include "fbufvdu.h"
#include <stdio.h>
#include <string.h>
#include "bbccon.h"

#if ( ! defined(PICO_SCANVIDEO_COLOR_PIN_BASE) ) || ( ! defined(PICO_SCANVIDEO_SYNC_PIN_BASE) )
#error SCANVIDEO pins not defined for VGA display
#endif

#include "font_tt.h"

void modechg (int mode);
void error (int iErr, const char *psErr);
void *oshwm (void *addr, int mark);

#define VGA_FLAG    0x1234                  // Used to syncronise cores

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

// Variables defined in fbufctl.c
extern int nCsrHide;                    // Non-zero to hide cursor
extern int csrhgt;                      // Height of cursor
extern int csrtop;                      // Top of cursor in text line

static bool bBlank = true;              // Blank video screen
static uint16_t renderbuf[256 * 8] __attribute((__aligned__(4)));

static MODE curmode;
static uint8_t  *framebuf = NULL;
static volatile uint8_t  *displaybuf = NULL;

static const uint16_t defpal[16] =
    {
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 128u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u, 128u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u,   0u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 128u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u, 128u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 64u,  64u,  64u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
    };

static const uint32_t ttcsr = PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
    | ( (PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)) << 16 );

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

#if HIRES    // 800x600 VGA
static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 640, 256,  80, 32,  44, 80, 8,  80, 1,  8},  // Mode  0 - 20KB
    { 2, 320, 256,  40, 32,  44, 80, 8,  80, 1,  8},  // Mode  1 - 20KB
    { 4, 160, 256,  20, 32,  44, 80, 8,  80, 1,  8},  // Mode  2 - 20KB
    { 1, 640, 250,  80, 25,  47, 80, 8,  80, 1, 10},  // Mode  3 - 20KB
    { 1, 320, 256,  40, 32,  44, 80, 8,  80, 1,  8},  // Mode  4 - 10KB
    { 2, 160, 256,  20, 32,  44, 80, 8,  80, 1,  8},  // Mode  5 - 10KB
    { 1, 320, 250,  40, 25,  44, 80, 8,  80, 1, 10},  // Mode  6 - 10KB
    { 3, 320, 225,  40, 25,  75, 80, 8,  80, 1,TTH},  // Mode  7 - Teletext TODO
    { 1, 640, 512,  80, 32,  44, 80, 8,  80, 0, 16},  // Mode  8 - 40KB
    { 2, 320, 512,  40, 32,  44, 80, 8,  80, 0, 16},  // Mode  9 - 40KB
    { 4, 160, 512,  20, 32,  44, 80, 8,  80, 0, 16},  // Mode 10 - 40KB
    { 1, 640, 500,  80, 25,  47, 80, 8,  80, 0, 20},  // Mode 11 - 40KB
    { 1, 320, 512,  40, 32,  44, 80, 8,  80, 0, 16},  // Mode 12 - 20KB
    { 2, 160, 512,  20, 32,  44, 80, 8,  80, 0, 16},  // Mode 13 - 20KB
    { 1, 320, 500,  40, 25,  44, 80, 8,  80, 0, 20},  // Mode 14 - 20KB
    { 1, 640, 512,  40, 25,  44, 80, 8,  80, 0, 16},  // Mode 15 - Teletext ?
    { 1, 800, 600, 100, 30,   0,  0, 8, 100, 0, 20},  // Mode 16 - 59KB
    { 2, 400, 600,  50, 30,   0,  0, 8, 100, 0, 20},  // Mode 17 - 59KB
    { 4, 200, 600,  25, 30,   0,  0, 8, 100, 0, 20},  // Mode 18 - 59KB
    { 2, 800, 300, 100, 30,   0,  0, 4, 200, 1, 20},  // Mode 19 - 59KB
    { 4, 400, 300,  50, 30,   0,  0, 4, 200, 1, 10}   // Mode 20 - 59KB
    };
#else   // 640x480 VGA
static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 640, 256,  80, 32, 112,  0,  8,  80, 0,  8},  // Mode  0 - 20KB
    { 2, 320, 256,  40, 32, 112,  0,  8,  80, 0,  8},  // Mode  1 - 20KB
    { 4, 160, 256,  20, 32, 112,  0,  8,  80, 0,  8},  // Mode  2 - 20KB
    { 1, 640, 225,  80, 25,  15,  0,  8,  80, 1,  9},  // Mode  3 - 20KB
    { 1, 320, 256,  40, 32, 112,  0, 16,  40, 0,  8},  // Mode  4 - 10KB
    { 2, 160, 256,  20, 32, 112,  0, 16,  40, 0,  8},  // Mode  5 - 10KB
    { 1, 320, 225,  40, 25,  15,  0, 16,  40, 1,  9},  // Mode  6 - 10KB
    { 3, 320, 225,  40, 25,  15,  0,  4, 160, 1,TTH},  // Mode  7 - ~1KB - Teletext
    { 1, 640, 480,  80, 30,   0,  0,  8,  80, 0, 16},  // Mode  8 - 37.5KB
    { 2, 320, 480,  40, 30,   0,  0,  8,  80, 0, 16},  // Mode  9 - 37.5KB
    { 4, 160, 480,  20, 30,   0,  0,  8,  80, 0, 16},  // Mode 10 - 37.5KB
    { 1, 640, 450,  80, 25,  15,  0,  8,  80, 0, 18},  // Mode 11 - 37.5KB
    { 1, 320, 480,  40, 30,   0,  0, 16,  40, 0, 16},  // Mode 12 - 18.75KB
    { 2, 160, 480,  20, 30,   0,  0, 16,  40, 0, 16},  // Mode 13 - 18.75KB
    { 1, 320, 450,  40, 25,  15,  0, 16,  40, 0, 18},  // Mode 14 - 18.75KB
    { 4, 320, 240,  40, 30,   0,  0,  4, 160, 1,  8},  // Mode 15 - 37.5KB
    };
#endif

/****** Routines executed on Core 1 **********************************************************

Video render routines must be entirely in RAM.
Do not use integer divide or modulus as compiler may turn these into calls
routines in Flash.

*/

#if USE_INTERP
static bool bCfgInt = false;    // Configure interpolators
extern void convert_from_pal16_16 (uint32_t *dest, uint8_t *src, uint32_t count);
extern void convert_from_pal16_8 (uint32_t *dest, uint8_t *src, uint32_t count);
extern void convert_from_pal16_4 (uint32_t *dest, uint8_t *src, uint32_t count);
extern void convert_from_pal16 (uint32_t *dest, uint8_t *src, uint32_t count);
#endif

#if HIRES
extern const scanvideo_timing_t vga_timing_800x600_60_default;
#endif
#define FLASH_BIT   0x20    // Bit in frame count used to control flash

void __time_critical_func(render_mode7) (void)
    {
    uint32_t nFrame = 0;
    uint32_t iRow;
    uint32_t iScanCnt;
    uint32_t iScanLst;
    bool bDouble = false;
    bool bLower = false;
    uint8_t *pttfont = &framebuf[curmode.trow * curmode.tcol] - 0x20 * TTH;
#if DEBUG & 1
    printf ("Entered mode 7 rendering\n");
#endif
    while (curmode.ncbt == 3)
        {
#if USE_INTERP
        if ( bCfgInt )
            {
            bCfgInt = false;
            bBlank = false;
#if DEBUG & 1
            printf ("Mode 7 display enabled\n");
#endif
            }
#endif
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        uint32_t *twopix = buffer->data;
        int iScan = scanvideo_scanline_number (buffer->scanline_id);
#if DBUF_MODE > 0
        if ( displaybuf && ( iScan == 0 ))
            {
            framebuf = (uint8_t *) displaybuf;
            displaybuf = NULL;;
            }
#endif
        iScan -= curmode.vmgn;
        if (( bBlank ) || (iScan < 0) || (iScan >= ( curmode.grow << curmode.yshf )))
            {
            twopix[0] = COMPOSABLE_COLOR_RUN;
            twopix[2] = SWIDTH - 2 | ( COMPOSABLE_EOL_ALIGN << 16 );
            buffer->data_used = 2;
            }
        else
            {
            if ( iScan == 0 )
                {
                ++nFrame;
                iRow = 0;
                iScanCnt = 0;
                iScanLst = 0;
                bDouble = false;
                bLower = false;
                }
            else
                {
                iScanCnt += iScan - iScanLst;
                if ( iScanCnt >= ( curmode.thgt << curmode.yshf ) )
                    {
                    iScanCnt -= curmode.thgt << curmode.yshf;
                    ++iRow;
                    if (bDouble) bLower = ! bLower;
                    else bLower = false;
                    bDouble = false;
                    }
                iScanLst = iScan;
                }
            iScan = iScanCnt >> curmode.yshf;
            uint8_t *pch = &framebuf[iRow * curmode.tcol];
            uint32_t *pxline = twopix;
            const uint8_t *pfont = pttfont;
            int nfg = 7;
            uint32_t bgnd = ((uint32_t *)renderbuf)[0];
            uint32_t fgnd = ((uint32_t *)renderbuf)[nfg];
            bool bFlash = false;
            bool bGraph = false;
            bool bCont = true;
            bool bHold = false;
            int iScan2 = iScan;
            for (int iCol = 0; iCol < curmode.tcol; ++iCol)
                {
                uint8_t ch = *pch & 0x7F;
                if ( ch >= 0x20 )
                    {
                    uint8_t px = pfont[TTH * ch + iScan2];
                    ++twopix;
                    if ( px & 0x01 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x02 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x04 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x08 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x10 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x20 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x40 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    ++twopix;
                    if ( px & 0x80 ) *twopix = fgnd;
                    else             *twopix = bgnd;
                    }
                else
                    {
                    if ( bHold )
                        {
                        twopix[0] = twopix[-8];
                        twopix[1] = twopix[-7];
                        twopix[2] = twopix[-6];
                        twopix[3] = twopix[-5];
                        twopix[4] = twopix[-4];
                        twopix[5] = twopix[-3];
                        twopix[6] = twopix[-2];
                        twopix[7] = twopix[-1];
                        }
                    else
                        {
                        twopix[0] = bgnd;
                        twopix[1] = bgnd;
                        twopix[2] = bgnd;
                        twopix[3] = bgnd;
                        twopix[4] = bgnd;
                        twopix[5] = bgnd;
                        twopix[6] = bgnd;
                        twopix[7] = bgnd;
                        }
                    twopix += 8;
                    if ((ch >= 0x00) && (ch <= 0x07))
                        {
                        pfont = pttfont;
                        bGraph = false;
                        nfg = ch & 0x07;
                        if (( bFlash ) && ( nFrame & FLASH_BIT ))
                            fgnd = bgnd;
                        else
                            fgnd = ((uint32_t *)renderbuf)[nfg];
                        }
                    else if (ch == 0x08)
                        {
                        bFlash = true;
                        if ( nFrame & FLASH_BIT )
                            fgnd = bgnd;
                        else
                            fgnd = ((uint32_t *)renderbuf)[nfg];
                        }
                    else if (ch == 0x09)
                        {
                        bFlash = false;
                        fgnd = ((uint32_t *)renderbuf)[nfg];
                        }
                    else if (ch == 0x0C)
                        {
                        iScan2 = iScan;
                        }
                    else if (ch == 0x0D)
                        {
                        if ( bLower )   iScan2 = ( iScan + TTH ) >> 1;
                        else            iScan2 = iScan >> 1;
                        bDouble = true;
                        }
                    else if ((ch >= 0x10) && (ch <= 0x17))
                        {
                        if ( bCont )    pfont = pttfont + 0x60 * TTH;
                        else            pfont = pttfont + 0xC0 * TTH;
                        bGraph = true;
                        nfg = ch & 0x07;
                        if (( bFlash ) && ( nFrame & FLASH_BIT ))
                            fgnd = bgnd;
                        else
                            fgnd = ((uint32_t *)renderbuf)[nfg];
                        }
                    else if (ch == 0x19)
                        {
                        bCont = true;
                        if ( bGraph ) pfont = pttfont + 0x60 * TTH;
                        }
                    else if (ch == 0x1A)
                        {
                        bCont = false;
                        if ( bGraph ) pfont = pttfont + 0xC0 * TTH;
                        }
                    else if (ch == 0x1C)
                        {
                        bgnd = ((uint32_t *)renderbuf)[0];
                        }
                    else if (ch == 0x1D)
                        {
                        bgnd = ((uint32_t *)renderbuf)[nfg];
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
                ++pch;
                }
            ++twopix;
            *twopix = COMPOSABLE_EOL_ALIGN << 16;   // Implicit zero (black) in low word
            ++twopix;
            buffer->data_used = twopix - buffer->data;
            if (( iRow == ycsr ) && ( nCsrHide == 0 ) && ( nFrame & FLASH_BIT )
                && ( iScan >= csrtop ) && ( iScan < csrtop + csrhgt ))
                {
                twopix = pxline + 8 * xcsr + 1;
                twopix[0] ^= ttcsr;
                twopix[1] ^= ttcsr;
                twopix[2] ^= ttcsr;
                twopix[3] ^= ttcsr;
                twopix[4] ^= ttcsr;
                twopix[5] ^= ttcsr;
                twopix[6] ^= ttcsr;
                twopix[7] ^= ttcsr;
                }
            pxline[0] = ( pxline[1] << 16 ) | COMPOSABLE_RAW_RUN;
            pxline[1] = ( pxline[1] & 0xFFFF0000 ) | ( curmode.nbpl * curmode.nppb - 2 );
            }
        scanvideo_end_scanline_generation (buffer);
        }
#if DEBUG & 1
    printf ("Quit mode 7 rendering\n");
#endif
    }

// The interpolators are core specific, so must be
// configured on the same core as the render_loop
#if USE_INTERP
static void setup_interp (void)
    {
    interp_config c = interp_default_config();
    if ( curmode.nppb == 16 )
        {
        interp_config_set_shift (&c, 20);
        interp_config_set_mask (&c, 4, 7);
        interp_set_config (interp0, 0, &c);
        interp_config_set_shift (&c, 24);
        }
    else if ( curmode.nppb == 8 )
        {
        interp_config_set_shift (&c, 20);
        interp_config_set_mask (&c, 4, 11);
        interp_set_config (interp0, 0, &c);
        interp_config_set_shift (&c, 12);
        }
    else if ( curmode.nppb == 4 )
        {
        interp_config_set_shift (&c, 21);
        interp_config_set_mask (&c, 3, 10);
        interp_set_config (interp0, 0, &c);
        interp_config_set_shift (&c, 13);
        }
    else
        {
        interp_config_set_shift (&c, 22);
        interp_config_set_mask (&c, 2, 9);
        interp_set_config (interp0, 0, &c);
        interp_config_set_shift (&c, 14);
        }
    interp_config_set_cross_input (&c, true);
    interp_set_config (interp0, 1, &c);
    interp_set_base (interp0, 0, (uintptr_t)renderbuf);
    interp_set_base (interp0, 1, (uintptr_t)renderbuf);
#if DEBUG & 2
    printf ("Interpolator test:\n");
    for (uint32_t i = 0; i < 256; ++i )
        {
        static uint16_t buf[32];
        static uint8_t dfb[4];
        dfb[0] = i;
        dfb[1] = i;
        dfb[2] = i;
        dfb[3] = i;
        convert_from_pal16_8 ((uint32_t *) buf, dfb, 4);
        printf ("interp (%3d) = I", i);
        for (int j = 0; j < 32; ++j)
            {
            if ( buf[j] ) printf ("*");
            else printf (" ");
            }
        printf ("I\n");
        }
#endif
    bCfgInt = false;
    bBlank = false;
    }
#endif

void __time_critical_func(render_loop) (void)
    {
    while (true)
        {
        if ( curmode.ncbt == 3 ) render_mode7 ();
#if USE_INTERP
        if ( bCfgInt ) setup_interp ();
#endif
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        uint32_t *twopix = buffer->data;
        int iScan = scanvideo_scanline_number (buffer->scanline_id);
#if DBUF_MODE > 0
        if ( displaybuf && ( iScan == 0 ))
            {
            framebuf = (uint8_t *) displaybuf;
            displaybuf = NULL;;
            }
#endif
        iScan -= curmode.vmgn;
        if (( bBlank ) || (iScan < 0) || (iScan >= ( curmode.grow << curmode.yshf )))
            {
            twopix[0] = COMPOSABLE_COLOR_RUN;
            twopix[2] = SWIDTH - 2 | ( COMPOSABLE_EOL_ALIGN << 16 );
            buffer->data_used = 2;
            }
        else
            {
            iScan >>= curmode.yshf;
            if ( curmode.hmgn > 0 )
                {
                *twopix = COMPOSABLE_COLOR_RUN;             // High word is zero, black pixel
                ++twopix;
                *twopix = (curmode.hmgn - 5) | COMPOSABLE_RAW_2P;    // To use an even number of 16-bit words
                ++twopix;
                *twopix = 0;                                // 2 black pixels
                }
            uint32_t *pxline = twopix;
            ++twopix;
            uint8_t *pfb = framebuf + iScan * curmode.nbpl;
            if ( curmode.nppb == 8 )
                {
#if USE_INTERP
                convert_from_pal16_8 (twopix, pfb, curmode.nbpl);
                twopix += 4 * curmode.nbpl;
#else
                for (int i = 0; i < curmode.nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[8 * (*pfb)];
                    *(twopix) = *twoclr;
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    ++twopix;
                    ++pfb;
                    }
#endif
                }
            else if ( curmode.nppb == 16 )
                {
#if USE_INTERP
                convert_from_pal16_16 (twopix, pfb, curmode.nbpl);
                twopix += 8 * curmode.nbpl;
#else
                for (int i = 0; i < curmode.nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[8 * ((*pfb) & 0x0F)];
                    *(twopix) = *twoclr;
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    twoclr = (uint32_t *) &renderbuf[8 * ((*pfb) >> 4)];
                    *(++twopix) = *twoclr;
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    *(++twopix) = *(++twoclr);
                    ++twopix;
                    ++pfb;
                    }
#endif
                }
            else if ( curmode.nppb == 4 )
                {
#if USE_INTERP
                convert_from_pal16_4 (twopix, pfb, curmode.nbpl);
                twopix += 2 * curmode.nbpl;
#else
                for (int i = 0; i < curmode.nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[4 * (*pfb)];
                    *(twopix) = *twoclr;
                    *(++twopix) = *(++twoclr);
                    ++twopix;
                    ++pfb;
                    }
#endif
                }
            else if ( curmode.nppb == 2 )
                {
#if USE_INTERP
                convert_from_pal16 (twopix, pfb, curmode.nbpl);
                twopix += curmode.nbpl;
#else
                for (int i = 0; i < curmode.nbpl; ++i)
                    {
                    uint32_t *twoclr = (uint32_t *) &renderbuf[2 * (*pfb)];
                    *(twopix) = *twoclr;
                    ++twopix;
                    ++pfb;
                    }
#endif
                }
            *twopix = COMPOSABLE_EOL_ALIGN << 16;   // Implicit zero (black) in low word
            ++twopix;
            pxline[0] = ( pxline[1] << 16 ) | COMPOSABLE_RAW_RUN;
            pxline[1] = ( pxline[1] & 0xFFFF0000 ) | ( curmode.nbpl * curmode.nppb - 2 );
            buffer->data_used = twopix - buffer->data;
            }
        scanvideo_end_scanline_generation (buffer);
        }
    }

// Replacement for the routine in the SDK which is not RAM resident
static uint __time_critical_func(scanline_repeat_count_fn)(uint32_t scanline_id)
    {
    return 1;
    }

void setup_video (void)
    {
#if DEBUG & 1
#if USE_INTERP
    printf ("setup_video: Using interpolator\n");
#if PICO_SDK_VERSION_MAJOR >= 2
    interp_claim_lane_mask (interp0, 0x03);
#endif
#endif
    printf ("setup_video: System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
    printf ("setup_video: Starting video\n");
#endif
#if HIRES
    scanvideo_setup (&vga_mode_800x600_60);
#else
    scanvideo_setup (&vga_mode_640x480_60);
#endif
    scanvideo_set_scanline_repeat_fn (scanline_repeat_count_fn);
    multicore_fifo_drain ();
    multicore_fifo_push_blocking (VGA_FLAG);
#ifdef PICO_MCLOCK
    multicore_lockout_victim_init ();
#endif
    scanvideo_timing_enable (true);
#if DEBUG & 1
    printf ("setup_video: System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
#endif
#if DEBUG & 1
    printf ("setup_video: Starting render\n");
#endif
    render_loop ();
    }

/****** Routines executed on Core 0 **********************************************************/

void gsize (uint32_t *pwth, uint32_t *phgt)
    {
    *pwth = 640;
    *phgt = 480;
    }

void bufswap (uint8_t *fbuf)
    {
    displaybuf = fbuf;
    while (displaybuf)
        {
        tight_loop_contents ();
        }
    }

uint16_t defclr (int clr)
    {
    return defpal[clr];
    }

uint16_t rgbclr (int r, int g, int b)
    {
    return PICO_SCANVIDEO_PIXEL_FROM_RGB8(r, g, b);
    }

int clrrgb (uint16_t clr)
    {
    return ( PICO_SCANVIDEO_R5_FROM_PIXEL(clr) << 3 )
        | ( PICO_SCANVIDEO_G5_FROM_PIXEL(clr) << 11 )
        | ( PICO_SCANVIDEO_B5_FROM_PIXEL(clr) << 19 );
    }

void genrb (uint16_t *curpal)
    {
    uint16_t *prb = (uint16_t *)renderbuf;
#if DEBUG & 1
    printf ("genrb\n");
#endif
    int nbuf = 256;
    int npix = 8 / curmode.ncbt;
    int nrpt = curmode.nppb / npix;
    uint32_t clrmsk = clrdef[curmode.ncbt].clrmsk;
    if ( curmode.nppb == 16 )
        {
        nbuf = 16;
        npix /= 2;
        }
    for (int i = 0; i < nbuf; ++i)
        {
        uint32_t ibuf = i;
        for (int j = 0; j < npix; ++j)
            {
            uint16_t clr = curpal[clrmsk & ibuf];
            for (int k = 0; k < nrpt; ++k)
                {
                *prb = clr;
                ++prb;
                }
            ibuf >>= curmode.ncbt;
            }
        }
#if DEBUG & 1
    printf ("End genrb: prb - renderbuf = %p - %p = %d\n", prb, renderbuf, prb - renderbuf);
#endif
    }

bool setmode (int mode, uint8_t **pfbuf, MODE **ppmd, CLRDEF **ppcd)
    {
#if DEBUG & 1
    printf ("setmode (%d)\n", mode);
#endif
    if (( mode >= 0 ) && ( mode < sizeof (modes) / sizeof (modes[0]) ))
        {
        bBlank = true;
        memcpy (&curmode, &modes[mode], sizeof (MODE));
        framebuf = singlebuf ();
        nCsrHide = CSR_OFF;
        csrtop = curmode.thgt - 1;
        csrhgt = 1;
        *pfbuf = framebuf;
        *ppmd = &curmode;
        *ppcd = &clrdef[curmode.ncbt];
        if ( curmode.ncbt == 3 ) memcpy (&framebuf[curmode.trow * curmode.tcol], font_tt, sizeof (font_tt));
        return true;
        }
    return false;
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
#if USE_INTERP
    bCfgInt = true;
#else
    bBlank = false;
#endif
#if DEBUG & 1
    printf ("dispmode: New mode set\n");
#endif
    }

bool txtmode (int code, int *pdata1, int *pdata2)
    {
    if ((code & 0x7F) >= sizeof (modes) / sizeof (modes[0])) return false;
    MODE *pmode = &modes[(code & 0x7F)];
    *pdata2 = (pmode->gcol << 8) | (pmode->grow << 24);
    *pdata1 = (pmode->grow >> 8) | 0x0800 | (pmode->thgt << 16) | (0x01000000 << pmode->ncbt);
    return true;
    }

int setup_vdu (void)
    {
#if DEBUG & 1
#if HIRES
    printf ("setup_vdu: 800x600 " __DATE__ " " __TIME__ "\n");
#else
    printf ("setup_vdu: 640x480 " __DATE__ " " __TIME__ "\n");
#endif
#if USE_INTERP
    printf ("setup_vdu: Using interpolator\n");
#else
    printf ("setup_vdu: Without interpolator\n");
#endif
    sleep_ms(500);
#endif
#if HIRES   // 800x600 VGA
    set_sys_clock_khz (8 * vga_timing_800x600_60_default.clock_freq / 1000, true);
#else       // 640x480 VGA
    set_sys_clock_khz (6 * vga_timing_640x480_60_default.clock_freq / 1000, true);
#endif
    stdio_init_all();
#if DEBUG & 1
    sleep_ms(500);
    printf ("setup_vdu: Set clock frequency %d kHz.\n", clock_get_hz (clk_sys) / 1000);
#endif
    setup_fbuf (&curmode);
    modechg (8);
    multicore_fifo_drain ();
    multicore_launch_core1 (setup_video);

    // Do not return until Core 1 has claimed DMA channels
    uint32_t test = multicore_fifo_pop_blocking ();
#if DEBUG & 1
    if ( test != VGA_FLAG )
        {
        printf ("Unexpected value 0x%04X from multicore FIFO\n", test);
        }
#endif
    }

void mode7flash (void)
    {
    }
