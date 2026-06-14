/*  framebuf.c - Implementation of a framebuffer for display of BBC BASIC VDU commands.

    The framebuffer uses 1, 2 or 4 bits per pixel, depending upon the number of colours.
    The framebuffer is strictly little-ended. Successive bytes are displayed left to right,
    and within each byte the least significant bits are displayed first.

    Mode 7 is the exception. In this case the buffer contains 7-bit character and control codes
    Bytes 0x00-0x1F are Teletext control codes and 0x20-0x7F are displayable characters.

    For the Pico in mode 7, the Teletext font is copied into the buffer following the displayed data
    in order to avoid video disruption when programming Flash memory. The font consists of
    three blocks of 96 glyphs for Alphanumeric, Continuous Graphics and Separated Graphics
    display.

*/

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "bbccon.h"
#include "framebuf.h"

#ifdef VDU_OUT
void VDU_OUT (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2);
#else
#define VDU_OUT(...)
#endif
#ifdef VDU_OUT_INT
void VDU_OUT_INT (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2);
#else
#define VDU_OUT_INT(...)
#endif
#ifdef VDU_OUT7
void VDU_OUT7 (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2);
#else
#define VDU_OUT7(...)
#endif
#ifdef VDU_SCROLL
void VDU_SCROLL (uint8_t *fbuf, int lt, int lb, bool bUp);
#endif
#ifdef VDU_SCROLL7
void VDU_SCROLL7 (uint8_t *fbuf, int lt, int lb, bool bUp);
#endif

#if REF_MODE == 3
RFM rfm = rfmNone;
#endif

static uint8_t *framebuf = NULL;
static const MODE *pmode = NULL;
static CLRDEF *cdef = NULL;                 // Colour definitions
static bool bCsrVis = false;
static int nCsrHide = 0;

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

static const uint8_t pmsk02[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

static const uint16_t pmsk04[256] = {
    0x0000, 0xC000, 0x3000, 0xF000, 0x0C00, 0xCC00, 0x3C00, 0xFC00,
    0x0300, 0xC300, 0x3300, 0xF300, 0x0F00, 0xCF00, 0x3F00, 0xFF00,
    0x00C0, 0xC0C0, 0x30C0, 0xF0C0, 0x0CC0, 0xCCC0, 0x3CC0, 0xFCC0,
    0x03C0, 0xC3C0, 0x33C0, 0xF3C0, 0x0FC0, 0xCFC0, 0x3FC0, 0xFFC0,
    0x0030, 0xC030, 0x3030, 0xF030, 0x0C30, 0xCC30, 0x3C30, 0xFC30,
    0x0330, 0xC330, 0x3330, 0xF330, 0x0F30, 0xCF30, 0x3F30, 0xFF30,
    0x00F0, 0xC0F0, 0x30F0, 0xF0F0, 0x0CF0, 0xCCF0, 0x3CF0, 0xFCF0,
    0x03F0, 0xC3F0, 0x33F0, 0xF3F0, 0x0FF0, 0xCFF0, 0x3FF0, 0xFFF0,
    0x000C, 0xC00C, 0x300C, 0xF00C, 0x0C0C, 0xCC0C, 0x3C0C, 0xFC0C,
    0x030C, 0xC30C, 0x330C, 0xF30C, 0x0F0C, 0xCF0C, 0x3F0C, 0xFF0C,
    0x00CC, 0xC0CC, 0x30CC, 0xF0CC, 0x0CCC, 0xCCCC, 0x3CCC, 0xFCCC,
    0x03CC, 0xC3CC, 0x33CC, 0xF3CC, 0x0FCC, 0xCFCC, 0x3FCC, 0xFFCC,
    0x003C, 0xC03C, 0x303C, 0xF03C, 0x0C3C, 0xCC3C, 0x3C3C, 0xFC3C,
    0x033C, 0xC33C, 0x333C, 0xF33C, 0x0F3C, 0xCF3C, 0x3F3C, 0xFF3C,
    0x00FC, 0xC0FC, 0x30FC, 0xF0FC, 0x0CFC, 0xCCFC, 0x3CFC, 0xFCFC,
    0x03FC, 0xC3FC, 0x33FC, 0xF3FC, 0x0FFC, 0xCFFC, 0x3FFC, 0xFFFC,
    0x0003, 0xC003, 0x3003, 0xF003, 0x0C03, 0xCC03, 0x3C03, 0xFC03,
    0x0303, 0xC303, 0x3303, 0xF303, 0x0F03, 0xCF03, 0x3F03, 0xFF03,
    0x00C3, 0xC0C3, 0x30C3, 0xF0C3, 0x0CC3, 0xCCC3, 0x3CC3, 0xFCC3,
    0x03C3, 0xC3C3, 0x33C3, 0xF3C3, 0x0FC3, 0xCFC3, 0x3FC3, 0xFFC3,
    0x0033, 0xC033, 0x3033, 0xF033, 0x0C33, 0xCC33, 0x3C33, 0xFC33,
    0x0333, 0xC333, 0x3333, 0xF333, 0x0F33, 0xCF33, 0x3F33, 0xFF33,
    0x00F3, 0xC0F3, 0x30F3, 0xF0F3, 0x0CF3, 0xCCF3, 0x3CF3, 0xFCF3,
    0x03F3, 0xC3F3, 0x33F3, 0xF3F3, 0x0FF3, 0xCFF3, 0x3FF3, 0xFFF3,
    0x000F, 0xC00F, 0x300F, 0xF00F, 0x0C0F, 0xCC0F, 0x3C0F, 0xFC0F,
    0x030F, 0xC30F, 0x330F, 0xF30F, 0x0F0F, 0xCF0F, 0x3F0F, 0xFF0F,
    0x00CF, 0xC0CF, 0x30CF, 0xF0CF, 0x0CCF, 0xCCCF, 0x3CCF, 0xFCCF,
    0x03CF, 0xC3CF, 0x33CF, 0xF3CF, 0x0FCF, 0xCFCF, 0x3FCF, 0xFFCF,
    0x003F, 0xC03F, 0x303F, 0xF03F, 0x0C3F, 0xCC3F, 0x3C3F, 0xFC3F,
    0x033F, 0xC33F, 0x333F, 0xF33F, 0x0F3F, 0xCF3F, 0x3F3F, 0xFF3F,
    0x00FF, 0xC0FF, 0x30FF, 0xF0FF, 0x0CFF, 0xCCFF, 0x3CFF, 0xFCFF,
    0x03FF, 0xC3FF, 0x33FF, 0xF3FF, 0x0FFF, 0xCFFF, 0x3FFF, 0xFFFF};

static const uint32_t pmsk16[256] = {
    0x00000000, 0xF0000000, 0x0F000000, 0xFF000000, 0x00F00000, 0xF0F00000, 0x0FF00000, 0xFFF00000,
    0x000F0000, 0xF00F0000, 0x0F0F0000, 0xFF0F0000, 0x00FF0000, 0xF0FF0000, 0x0FFF0000, 0xFFFF0000,
    0x0000F000, 0xF000F000, 0x0F00F000, 0xFF00F000, 0x00F0F000, 0xF0F0F000, 0x0FF0F000, 0xFFF0F000,
    0x000FF000, 0xF00FF000, 0x0F0FF000, 0xFF0FF000, 0x00FFF000, 0xF0FFF000, 0x0FFFF000, 0xFFFFF000,
    0x00000F00, 0xF0000F00, 0x0F000F00, 0xFF000F00, 0x00F00F00, 0xF0F00F00, 0x0FF00F00, 0xFFF00F00,
    0x000F0F00, 0xF00F0F00, 0x0F0F0F00, 0xFF0F0F00, 0x00FF0F00, 0xF0FF0F00, 0x0FFF0F00, 0xFFFF0F00,
    0x0000FF00, 0xF000FF00, 0x0F00FF00, 0xFF00FF00, 0x00F0FF00, 0xF0F0FF00, 0x0FF0FF00, 0xFFF0FF00,
    0x000FFF00, 0xF00FFF00, 0x0F0FFF00, 0xFF0FFF00, 0x00FFFF00, 0xF0FFFF00, 0x0FFFFF00, 0xFFFFFF00,
    0x000000F0, 0xF00000F0, 0x0F0000F0, 0xFF0000F0, 0x00F000F0, 0xF0F000F0, 0x0FF000F0, 0xFFF000F0,
    0x000F00F0, 0xF00F00F0, 0x0F0F00F0, 0xFF0F00F0, 0x00FF00F0, 0xF0FF00F0, 0x0FFF00F0, 0xFFFF00F0,
    0x0000F0F0, 0xF000F0F0, 0x0F00F0F0, 0xFF00F0F0, 0x00F0F0F0, 0xF0F0F0F0, 0x0FF0F0F0, 0xFFF0F0F0,
    0x000FF0F0, 0xF00FF0F0, 0x0F0FF0F0, 0xFF0FF0F0, 0x00FFF0F0, 0xF0FFF0F0, 0x0FFFF0F0, 0xFFFFF0F0,
    0x00000FF0, 0xF0000FF0, 0x0F000FF0, 0xFF000FF0, 0x00F00FF0, 0xF0F00FF0, 0x0FF00FF0, 0xFFF00FF0,
    0x000F0FF0, 0xF00F0FF0, 0x0F0F0FF0, 0xFF0F0FF0, 0x00FF0FF0, 0xF0FF0FF0, 0x0FFF0FF0, 0xFFFF0FF0,
    0x0000FFF0, 0xF000FFF0, 0x0F00FFF0, 0xFF00FFF0, 0x00F0FFF0, 0xF0F0FFF0, 0x0FF0FFF0, 0xFFF0FFF0,
    0x000FFFF0, 0xF00FFFF0, 0x0F0FFFF0, 0xFF0FFFF0, 0x00FFFFF0, 0xF0FFFFF0, 0x0FFFFFF0, 0xFFFFFFF0,
    0x0000000F, 0xF000000F, 0x0F00000F, 0xFF00000F, 0x00F0000F, 0xF0F0000F, 0x0FF0000F, 0xFFF0000F,
    0x000F000F, 0xF00F000F, 0x0F0F000F, 0xFF0F000F, 0x00FF000F, 0xF0FF000F, 0x0FFF000F, 0xFFFF000F,
    0x0000F00F, 0xF000F00F, 0x0F00F00F, 0xFF00F00F, 0x00F0F00F, 0xF0F0F00F, 0x0FF0F00F, 0xFFF0F00F,
    0x000FF00F, 0xF00FF00F, 0x0F0FF00F, 0xFF0FF00F, 0x00FFF00F, 0xF0FFF00F, 0x0FFFF00F, 0xFFFFF00F,
    0x00000F0F, 0xF0000F0F, 0x0F000F0F, 0xFF000F0F, 0x00F00F0F, 0xF0F00F0F, 0x0FF00F0F, 0xFFF00F0F,
    0x000F0F0F, 0xF00F0F0F, 0x0F0F0F0F, 0xFF0F0F0F, 0x00FF0F0F, 0xF0FF0F0F, 0x0FFF0F0F, 0xFFFF0F0F,
    0x0000FF0F, 0xF000FF0F, 0x0F00FF0F, 0xFF00FF0F, 0x00F0FF0F, 0xF0F0FF0F, 0x0FF0FF0F, 0xFFF0FF0F,
    0x000FFF0F, 0xF00FFF0F, 0x0F0FFF0F, 0xFF0FFF0F, 0x00FFFF0F, 0xF0FFFF0F, 0x0FFFFF0F, 0xFFFFFF0F,
    0x000000FF, 0xF00000FF, 0x0F0000FF, 0xFF0000FF, 0x00F000FF, 0xF0F000FF, 0x0FF000FF, 0xFFF000FF,
    0x000F00FF, 0xF00F00FF, 0x0F0F00FF, 0xFF0F00FF, 0x00FF00FF, 0xF0FF00FF, 0x0FFF00FF, 0xFFFF00FF,
    0x0000F0FF, 0xF000F0FF, 0x0F00F0FF, 0xFF00F0FF, 0x00F0F0FF, 0xF0F0F0FF, 0x0FF0F0FF, 0xFFF0F0FF,
    0x000FF0FF, 0xF00FF0FF, 0x0F0FF0FF, 0xFF0FF0FF, 0x00FFF0FF, 0xF0FFF0FF, 0x0FFFF0FF, 0xFFFFF0FF,
    0x00000FFF, 0xF0000FFF, 0x0F000FFF, 0xFF000FFF, 0x00F00FFF, 0xF0F00FFF, 0x0FF00FFF, 0xFFF00FFF,
    0x000F0FFF, 0xF00F0FFF, 0x0F0F0FFF, 0xFF0F0FFF, 0x00FF0FFF, 0xF0FF0FFF, 0x0FFF0FFF, 0xFFFF0FFF,
    0x0000FFFF, 0xF000FFFF, 0x0F00FFFF, 0xFF00FFFF, 0x00F0FFFF, 0xF0F0FFFF, 0x0FF0FFFF, 0xFFF0FFFF,
    0x000FFFFF, 0xF00FFFFF, 0x0F0FFFFF, 0xFF0FFFFF, 0x00FFFFFF, 0xF0FFFFFF, 0x0FFFFFFF, 0xFFFFFFFF};
    
static const uint32_t fwdmsk[] = {
    0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8, 0xFFFFFFF0, 0xFFFFFFE0, 0xFFFFFFC0, 0xFFFFFF80,
    0xFFFFFF00, 0xFFFFFE00, 0xFFFFFC00, 0xFFFFF800, 0xFFFFF000, 0xFFFFE000, 0xFFFFC000, 0xFFFF8000,
    0xFFFF0000, 0xFFFE0000, 0xFFFC0000, 0xFFF80000, 0xFFF00000, 0xFFE00000, 0xFFC00000, 0xFF800000,
    0xFF000000, 0xFE000000, 0xFC000000, 0xF8000000, 0xF0000000, 0xE0000000, 0XC0000000, 0x80000000 };
static const uint32_t bkwmsk[] = {
    0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF };

void fbmode (uint8_t *fb, MODE *pm)
    {
    framebuf = fb;
    pmode = pm;
    cdef = &clrdef[pmode->ncbt];
    }

#if SOFT_CSR
static void flipcsr (int xp, int yp, int ya, int yb)
    {
    yp += ya;
    int xpc = xp;
    int ypc = yp;
    uint32_t *fb = (uint32_t *)(framebuf + yp * pmode->nbpl);
    xp <<= cdef->bitsh;
    fb += xp >> 5;
    xp &= 0x1F;
    uint32_t msk1 = cdef->csrmsk << xp;
    uint32_t msk2 = cdef->csrmsk >> ( 32 - xp );
    for (int i = 0; i < yb - ya + 1; ++i)
        {
        *fb ^= msk1;
        *(fb + 1) ^= msk2;
        fb += pmode->nbpl / 4;
        ++yp;
        }
    VDU_OUT_INT (framebuf, xpc, ypc, xpc + 8, ypc + yb - ya + 1);
    }

void hidecsr (void)
    {
    ++nCsrHide;
    if ( pmode->ncbt != 3 )
        {
        int xp;
        int yp;
        if (! csrpos (&xp, &yp))
            {
            nCsrHide |= CSR_INV;
            return;
            }
        critical_section_enter_blocking (&cs_csr);
        if ( bCsrVis )
            {
            flipcsr (xp, yp, cursa, cursb);
            if (xccsr >= 0) flipcsr (8 * xccsr, pmode->thgt * yccsr, cursb, cursb);
            bCsrVis = ! bCsrVis;
            }
        critical_section_exit (&cs_csr);
        }
    }

void showcsr (void)
    {
    int xp;
    int yp;
    if ( ( nCsrHide & CSR_CNT ) > 0 ) --nCsrHide;
    if (csrpos (&xp, &yp))  nCsrHide &= ~CSR_INV;
    else                    nCsrHide |= CSR_INV;
    if ( nCsrHide == 0 )
        {
        if ( pmode->ncbt != 3 )
            {
            critical_section_enter_blocking (&cs_csr);
            if ( ! bCsrVis )
                {
                flipcsr (xp, yp, cursa, cursb);
                if (xccsr >= 0) flipcsr (8 * xccsr, pmode->thgt * yccsr, cursb, cursb);
                bCsrVis = ! bCsrVis;
                }
            critical_section_exit (&cs_csr);
            }
        }
    }

void flashcsr (void)
    {
    if ( pmode->ncbt == 3 )
        {
        mode7flash ();
        }
    else if ( nCsrHide == 0 )
        {
        int xp;
        int yp;
        if (csrpos (&xp, &yp))
            {
            critical_section_enter_blocking (&cs_csr);
            flipcsr (xp, yp, cursa, cursb);
            if (xccsr >= 0) flipcsr (8 * xccsr, pmode->thgt * yccsr, cursb, cursb);
            bCsrVis = ! bCsrVis;
            critical_section_exit (&cs_csr);
            }
        }
    }

void enablecsr (bool bEnable)
    {
    if (bEnable)    nCsrHide &= ~CSR_OFF;
    else            nCsrHide |= CSR_OFF;
    }
#endif

void scrldn (void)
    {
    hidecsr ();
    if ( pmode->ncbt == 3 )
        {
        if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
            {
            memmove (framebuf + ( tvt + 1 ) * pmode->tcol,
                framebuf + tvt * pmode->tcol,
                ( tvb - tvt ) * pmode->tcol);
            memset (framebuf + tvt * pmode->tcol, ' ', pmode->tcol);
            }
        else
            {
            uint8_t *fb1 = framebuf + tvb * pmode->tcol + tvl;
            uint8_t *fb2 = fb1 + pmode->tcol;
            int nr = tvb - tvt;
            int nb = tvr - tvl + 1;
            for (int ir = 0; ir < nr; ++ir)
                {
                fb1 -= pmode->tcol;
                fb2 -= pmode->tcol;
                memcpy (fb2, fb1, nb);
                }
            memset (fb1, ' ', nb);
            }
#ifdef VDU_SCROLL7
        VDU_SCROLL7 (framebuf, tvt, tvb, false);
#else
        VDU_OUT7 (framebuf, tvl, tvt, tvr, tvb);
#endif
        }
    else if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
        {
        uint8_t bgfill = (uint8_t) cdef->cpx[txtbak];
        memmove (framebuf + ( tvt + 1 ) * pmode->thgt * pmode->nbpl,
            framebuf + tvt * pmode->thgt * pmode->nbpl,
            ( tvb - tvt ) * pmode->thgt * pmode->nbpl);
        memset (framebuf + tvt * pmode->thgt * pmode->nbpl, bgfill, pmode->thgt * pmode->nbpl);
#ifdef VDU_SCROLL
        VDU_SCROLL (framebuf, tvt, tvb, false);
#else
        VDU_OUT (framebuf, tvl << 3, tvt * pmode->thgt, (tvr + 1) << 3, (tvb + 1) * pmode->thgt);
#endif
        }
    else
        {
        uint8_t bgfill = (uint8_t) cdef->cpx[txtbak];
        uint8_t *fb1 = framebuf + tvb * pmode->thgt * pmode->nbpl
            + ( tvl << cdef->bitsh );
        uint8_t *fb2 = fb1 + pmode->thgt * pmode->nbpl;
        int nr = ( tvb - tvt ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << cdef->bitsh;
        for (int ir = 0; ir < nr; ++ir)
            {
            fb1 -= pmode->nbpl;
            fb2 -= pmode->nbpl;
            memcpy (fb2, fb1, nb);
            }
        for (int ir = 0; ir < pmode->thgt; ++ir)
            {
            fb2 -= pmode->nbpl;
            memset (fb2, bgfill, nb);
            }
        VDU_OUT (framebuf, tvl << 3, tvt * pmode->thgt, (tvr + 1) << 3, (tvb + 1) * pmode->thgt);
        }
    showcsr ();
    }

void scrlup (void)
    {
    hidecsr ();
    if ( pmode->ncbt == 3 )
        {
        if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
            {
            memmove (framebuf, framebuf + pmode->tcol, ( pmode->trow - 1 ) * pmode->tcol);
            memset (framebuf + ( pmode->trow - 1 ) * pmode->tcol, ' ', pmode->tcol);
            }
        else
            {
            uint8_t *fb1 = framebuf + tvt * pmode->tcol + tvl;
            uint8_t *fb2 = fb1 + pmode->tcol;
            int nr = tvb - tvt;
            int nb = tvr - tvl + 1;
            for (int ir = 0; ir < nr; ++ir)
                {
                memcpy (fb1, fb2, nb);
                fb1 += pmode->tcol;
                fb2 += pmode->tcol;
                }
            memset (fb2, ' ', nb);
            }
#ifdef VDU_SCROLL7
        VDU_SCROLL7 (framebuf, tvt, tvb, true);
#else
        VDU_OUT7 (framebuf, tvl, tvt, tvr, tvb);
#endif
        }
    else if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
        {
        uint8_t bgfill = (uint8_t) cdef->cpx[txtbak];
        memmove (framebuf + tvt * pmode->thgt * pmode->nbpl,
            framebuf + ( tvt + 1 ) * pmode->thgt * pmode->nbpl,
            ( tvb - tvt ) * pmode->thgt * pmode->nbpl);
        memset (framebuf + tvb * pmode->thgt * pmode->nbpl, bgfill, pmode->thgt * pmode->nbpl);
#ifdef VDU_SCROLL
        VDU_SCROLL (framebuf, tvt, tvb, true);
#else
        VDU_OUT (framebuf, tvl << 3, tvt * pmode->thgt, (tvr + 1) << 3, (tvb + 1) * pmode->thgt);
#endif
        }
    else
        {
        uint8_t bgfill = (uint8_t) cdef->cpx[txtbak];
        uint8_t *fb1 = framebuf + tvt * pmode->thgt * pmode->nbpl
            + ( tvl << cdef->bitsh );
        uint8_t *fb2 = fb1 + pmode->thgt * pmode->nbpl;
        int nr = ( tvb - tvt ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << cdef->bitsh;
        for (int ir = 0; ir < nr; ++ir)
            {
            memcpy (fb1, fb2, nb);
            fb1 += pmode->nbpl;
            fb2 += pmode->nbpl;
            }
        for (int ir = 0; ir < pmode->thgt; ++ir)
            {
            memset (fb1, bgfill, nb);
            fb1 += pmode->nbpl;
            }
        VDU_OUT (framebuf, tvl << 3, tvt * pmode->thgt, (tvr + 1) << 3, (tvb + 1) * pmode->thgt);
        }
    showcsr ();
    }

void cls ()
    {
    hidecsr ();
    if ( pmode->ncbt == 3 )
        {
        if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
            {
            memset (framebuf + tvt * pmode->tcol, ' ', ( tvb - tvt + 1 ) * pmode->tcol);
            }
        else
            {
            uint8_t *fb1 = framebuf + tvt * pmode->tcol + tvl;
            int nr = tvb - tvt + 1;
            int nb = tvr - tvl + 1;
            for (int ir = 0; ir < nr; ++ir)
                {
                memset (fb1, ' ', nb);
                fb1 += pmode->tcol;
                }
            }
        VDU_OUT7 (framebuf, tvl, tvt, tvr, tvb);
        }
    else if (( tvl == 0 ) && ( tvr == pmode->tcol - 1 ))
        {
        uint8_t bgfill = (uint8_t) cdef->cpx[txtbak];
        memset (framebuf + tvt * pmode->thgt * pmode->nbpl, bgfill,
            ( tvb - tvt + 1 ) * pmode->thgt * pmode->nbpl);
        VDU_OUT (framebuf, tvl << 3, tvt * pmode->thgt, (tvr + 1) << 3, (tvb + 1) * pmode->thgt);
        }
    else
        {
        uint8_t bgfill = (uint8_t) cdef->cpx[txtbak];
        uint8_t *fb1 = framebuf + tvt * pmode->thgt * pmode->nbpl
            + ( tvl << cdef->bitsh );
        int nr = ( tvb - tvt + 1 ) * pmode->thgt;
        int nb = ( tvr - tvl + 1 ) << cdef->bitsh;
        for (int ir = 0; ir < nr; ++ir)
            {
            memset (fb1, bgfill, nb);
            fb1 += pmode->nbpl;
            }
        VDU_OUT (framebuf, tvl << 3, tvt * pmode->thgt, (tvr + 1) << 3, (tvb + 1) * pmode->thgt);
        }
    home ();
    showcsr ();
    }

void disp_ttx (char chr)
    {
    framebuf[ycsr * pmode->tcol + xcsr] = chr & 0x7F;
    VDU_OUT7 (framebuf, xcsr, ycsr, xcsr, ycsr);
    }

void disp_glyph (uint8_t *pch)
    {
    uint32_t fhgt = pmode->thgt;
    bool bDbl = false;
    if ( fhgt > 10 )
        {
        fhgt /= 2;
        bDbl = true;
        }
    fhgt = 8;
    uint8_t *pfb = framebuf + ycsr * pmode->thgt * pmode->nbpl;
    if ( pmode->ncbt == 1 )
        {
        pfb += xcsr;
        uint8_t fpx = (uint8_t) cdef->cpx[txtfor];
        uint8_t bpx = (uint8_t) cdef->cpx[txtbak];
        for (int i = 0; i < fhgt; ++i)
            {
            uint8_t mask = pmsk02[*pch];
            uint8_t pix = ( mask & fpx ) | ( (~ mask ) & bpx );
            *pfb = pix;
            pfb += pmode->nbpl;
            if ( bDbl )
                {
                *pfb = pix;
                pfb += pmode->nbpl;
                }
            ++pch;
            }
        }
    else if ( pmode->ncbt == 2 )
        {
        pfb += 2 * xcsr;
        uint16_t fpx = cdef->cpx[txtfor];
        uint16_t bpx = cdef->cpx[txtbak];
        for (int i = 0; i < fhgt; ++i)
            {
            uint16_t mask = pmsk04[*pch];
            uint16_t pix = ( mask & fpx ) | ( (~ mask) & bpx );
            *((uint16_t *)pfb) = pix;
            pfb += pmode->nbpl;
            if ( bDbl )
                {
                *((uint16_t *)pfb) = pix;
                pfb += pmode->nbpl;
                }
            ++pch;
            }
        }
    else if ( pmode->ncbt == 4 )
        {
        pfb += 4 * xcsr;
        uint32_t fpx = cdef->cpx[txtfor];
        uint32_t bpx = cdef->cpx[txtbak];
        for (int i = 0; i < fhgt; ++i)
            {
            uint32_t mask = pmsk16[*pch];
            uint32_t pix = ( mask & fpx ) | ( (~ mask) & bpx );
            *((uint32_t *)pfb) = pix;
            pfb += pmode->nbpl;
            if ( bDbl )
                {
                *((uint32_t *)pfb) = pix;
                pfb += pmode->nbpl;
                }
            ++pch;
            }
        }
    VDU_OUT (framebuf, xcsr << 3, ycsr * pmode->thgt, (xcsr + 1) << 3, (ycsr + 1) * pmode->thgt);
    }

static inline int clrmsk (int clr)
    {
    return ( clr & cdef->clrmsk );
    }

static inline void pixop (int op, uint32_t *fb, uint32_t msk, uint32_t cpx)
    {
#if DEBUG & 4
    printf ("pixop (%d, %p, 0x%08X, 0x%08X)\n", op, fb, msk, cpx);
#endif
    cpx &= msk;
    switch (op)
        {
        case 1:
            *fb |= cpx;
            break;
        case 2:
            *fb &= cpx | ~msk;
            break;
        case 3:
            *fb ^= cpx;
            break;
        case 4:
            *fb ^= msk;
            break;
        default:
            *fb &= ~msk;
            *fb |= cpx;
            break;
        }
    }

void hline (int clrop, int xp1, int xp2, int yp)
    {
    int op = clrop >> 8;
    int xb1 = xp1 << cdef->bitsh;
    int xb2 = xp2 << cdef->bitsh;
    uint32_t *fb1 = (uint32_t *)(framebuf + yp * pmode->nbpl);
    uint32_t *fb2 = fb1 + ( xb2 >> 5 );
    fb1 += ( xb1 >> 5 );
    uint32_t msk1 = fwdmsk[xb1 & 0x1F];
    uint32_t msk2 = bkwmsk[(xb2 & 0x1F) + pmode->ncbt - 1];
    uint32_t cpx = cdef->cpx[clrop & cdef->clrmsk];
    if ( fb2 == fb1 )
        {
        pixop (op, fb1, msk1 & msk2, cpx);
        }
    else
        {
        pixop (op, fb1, msk1, cpx);
        ++fb1;
        while ( fb1 < fb2 )
            {
            pixop (op, fb1, 0xFFFFFFFF, cpx);
            ++fb1;
            }
        pixop (op, fb1, msk2, cpx);
        }
    VDU_OUT (framebuf, xp1, yp, xp2 + 1, yp + 1);
    }

void clrgraph (void)
    {
    for (int yp = gvt; yp <= gvb; ++yp)
        {
        hline (bakgnd, gvl, gvr, yp);
        }
    }

void point (int clrop, uint32_t xp, uint32_t yp)
    {
#if DEBUG & 4
    printf ("point (0x%04X, %d, %d)\n", clrop, xp, yp);
#endif
    uint32_t *fb = (uint32_t *)(framebuf + yp * pmode->nbpl);
    uint32_t xb = xp << cdef->bitsh;
    fb += xb >> 5;
    xb &= 0x1F;
    uint32_t msk = cdef->clrmsk << xb;
    uint32_t cpx = ( clrop & cdef->clrmsk ) << xb;
    pixop (clrop >> 8, fb, msk, cpx);
#if DEBUG & 4
    printf ("xp = %d, cpx = %08X, msk = %08X, fb = %p, *fb = %08X\n", xp, cpx, msk, fb, *fb);
#endif
    VDU_OUT (framebuf, xp, yp, xp + 1, yp + 1);
    }

uint8_t getpix (int xp, int yp)
    {
    uint32_t *fb = (uint32_t *)(framebuf + yp * pmode->nbpl);
    xp <<= cdef->bitsh;
    fb += xp >> 5;
    xp &= 0x1F;
    uint8_t pix = (*fb) >> xp;
    pix &= cdef->clrmsk;
    return pix;
    }

int get_ttx (int x, int y)
    {
    int chr = framebuf[y * pmode->tcol + x];
    if ( chr < 0x20 ) chr += 0x80;
    return chr;
    }

void get_glyph (int x, int y, uint8_t *prow)
    {
    int bgclr;
    bool bFirst = true;
    int fhgt = pmode->thgt;
    bool bDbl = false;
    if ( fhgt > 10 )
        {
        fhgt /= 2;
        bDbl = true;
        }
    fhgt = 8;
    x *= 8;
    y *= pmode->thgt;
    for (int j = 0; j < fhgt; ++j)
        {
        for (int i = 0; i < 8; ++i)
            {
            int pclr = getpix (x+i, y);
            if (bFirst)
                {
                bgclr = pclr;
                bFirst = false;
                }
            *prow <<= 1;
            if ( pclr != bgclr ) *prow |= 0x01;
            }
        ++y;
        if ( bDbl ) ++y;
        ++prow;
        }
    }

#if REF_MODE == 1
void refresh_now (void)
    {
#if DEBUG & 4
    printf ("refresh now\n");
#endif
    if (reflag == 1) framebuf = swapbuf ();
    }

void refresh_on (void)
    {
#if DEBUG & 2
    printf ("refresh on\n");
#endif
    if (reflag == 1) framebuf = singlebuf ();
    reflag = 2;
    }

void refresh_off (void)
    {
#if DEBUG & 2
    printf ("refresh off\n");
#endif
    if (reflag != 1) framebuf = doublebuf ();
    reflag = 1;
    }

void refresh_rst (void)
    {
    refresh_on ();
    }

#elif REF_MODE == 3
void refresh_now (void)
    {
#if DEBUG & 4
    printf ("refresh now\n");
#endif
    if (reflag == 1)
        {
        if ( rfm == rfmBuffer )
            {
            framebuf = swapbuf ();
            }
        else if ( rfm == rfmQueue )
            {
            vduflush ();
            }
        }
    }

void refresh_on (void)
    {
#if DEBUG & 2
    printf ("refresh on\n");
#endif
    if (reflag == 1)
        {
        if ( rfm == rfmBuffer )
            {
            framebuf = singlebuf ();
            }
        else if ( rfm == rfmQueue )
            {
            vduflush ();
            vduqterm ();
            }
        }
    reflag = 2;
    }

void refresh_off (void)
    {
#if DEBUG & 2
    printf ("refresh off\n");
#endif
    if (reflag != 1)
        {
        if ( rfm == rfmBuffer )
            {
            framebuf = doublebuf ();
            }
        else if ( rfm == rfmQueue )
            {
            vduqinit ();
            }
        reflag = 1;
        }
    }

void refresh_rst (void)
    {
    refresh_on ();
    }
#endif

static const uint8_t swap04[256] = {
    0x00, 0x40, 0x80, 0xC0, 0x10, 0x50, 0x90, 0xD0, 0x20, 0x60, 0xA0, 0xE0, 0x30, 0x70, 0xB0, 0xF0,
    0x04, 0x44, 0x84, 0xC4, 0x14, 0x54, 0x94, 0xD4, 0x24, 0x64, 0xA4, 0xE4, 0x34, 0x74, 0xB4, 0xF4,
    0x08, 0x48, 0x88, 0xC8, 0x18, 0x58, 0x98, 0xD8, 0x28, 0x68, 0xA8, 0xE8, 0x38, 0x78, 0xB8, 0xF8,
    0x0C, 0x4C, 0x8C, 0xCC, 0x1C, 0x5C, 0x9C, 0xDC, 0x2C, 0x6C, 0xAC, 0xEC, 0x3C, 0x7C, 0xBC, 0xFC,
    0x01, 0x41, 0x81, 0xC1, 0x11, 0x51, 0x91, 0xD1, 0x21, 0x61, 0xA1, 0xE1, 0x31, 0x71, 0xB1, 0xF1,
    0x05, 0x45, 0x85, 0xC5, 0x15, 0x55, 0x95, 0xD5, 0x25, 0x65, 0xA5, 0xE5, 0x35, 0x75, 0xB5, 0xF5,
    0x09, 0x49, 0x89, 0xC9, 0x19, 0x59, 0x99, 0xD9, 0x29, 0x69, 0xA9, 0xE9, 0x39, 0x79, 0xB9, 0xF9,
    0x0D, 0x4D, 0x8D, 0xCD, 0x1D, 0x5D, 0x9D, 0xDD, 0x2D, 0x6D, 0xAD, 0xED, 0x3D, 0x7D, 0xBD, 0xFD,
    0x02, 0x42, 0x82, 0xC2, 0x12, 0x52, 0x92, 0xD2, 0x22, 0x62, 0xA2, 0xE2, 0x32, 0x72, 0xB2, 0xF2,
    0x06, 0x46, 0x86, 0xC6, 0x16, 0x56, 0x96, 0xD6, 0x26, 0x66, 0xA6, 0xE6, 0x36, 0x76, 0xB6, 0xF6,
    0x0A, 0x4A, 0x8A, 0xCA, 0x1A, 0x5A, 0x9A, 0xDA, 0x2A, 0x6A, 0xAA, 0xEA, 0x3A, 0x7A, 0xBA, 0xFA,
    0x0E, 0x4E, 0x8E, 0xCE, 0x1E, 0x5E, 0x9E, 0xDE, 0x2E, 0x6E, 0xAE, 0xEE, 0x3E, 0x7E, 0xBE, 0xFE,
    0x03, 0x43, 0x83, 0xC3, 0x13, 0x53, 0x93, 0xD3, 0x23, 0x63, 0xA3, 0xE3, 0x33, 0x73, 0xB3, 0xF3,
    0x07, 0x47, 0x87, 0xC7, 0x17, 0x57, 0x97, 0xD7, 0x27, 0x67, 0xA7, 0xE7, 0x37, 0x77, 0xB7, 0xF7,
    0x0B, 0x4B, 0x8B, 0xCB, 0x1B, 0x5B, 0x9B, 0xDB, 0x2B, 0x6B, 0xAB, 0xEB, 0x3B, 0x7B, 0xBB, 0xFB,
    0x0F, 0x4F, 0x8F, 0xCF, 0x1F, 0x5F, 0x9F, 0xDF, 0x2F, 0x6F, 0xAF, 0xEF, 0x3F, 0x7F, 0xBF, 0xFF };

static const uint8_t swap16[256] = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
    0x01, 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81, 0x91, 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1,
    0x02, 0x12, 0x22, 0x32, 0x42, 0x52, 0x62, 0x72, 0x82, 0x92, 0xA2, 0xB2, 0xC2, 0xD2, 0xE2, 0xF2,
    0x03, 0x13, 0x23, 0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3, 0xF3,
    0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4, 0xB4, 0xC4, 0xD4, 0xE4, 0xF4,
    0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65, 0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
    0x06, 0x16, 0x26, 0x36, 0x46, 0x56, 0x66, 0x76, 0x86, 0x96, 0xA6, 0xB6, 0xC6, 0xD6, 0xE6, 0xF6,
    0x07, 0x17, 0x27, 0x37, 0x47, 0x57, 0x67, 0x77, 0x87, 0x97, 0xA7, 0xB7, 0xC7, 0xD7, 0xE7, 0xF7,
    0x08, 0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78, 0x88, 0x98, 0xA8, 0xB8, 0xC8, 0xD8, 0xE8, 0xF8,
    0x09, 0x19, 0x29, 0x39, 0x49, 0x59, 0x69, 0x79, 0x89, 0x99, 0xA9, 0xB9, 0xC9, 0xD9, 0xE9, 0xF9,
    0x0A, 0x1A, 0x2A, 0x3A, 0x4A, 0x5A, 0x6A, 0x7A, 0x8A, 0x9A, 0xAA, 0xBA, 0xCA, 0xDA, 0xEA, 0xFA,
    0x0B, 0x1B, 0x2B, 0x3B, 0x4B, 0x5B, 0x6B, 0x7B, 0x8B, 0x9B, 0xAB, 0xBB, 0xCB, 0xDB, 0xEB, 0xFB,
    0x0C, 0x1C, 0x2C, 0x3C, 0x4C, 0x5C, 0x6C, 0x7C, 0x8C, 0x9C, 0xAC, 0xBC, 0xCC, 0xDC, 0xEC, 0xFC,
    0x0D, 0x1D, 0x2D, 0x3D, 0x4D, 0x5D, 0x6D, 0x7D, 0x8D, 0x9D, 0xAD, 0xBD, 0xCD, 0xDD, 0xED, 0xFD,
    0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0x8E, 0x9E, 0xAE, 0xBE, 0xCE, 0xDE, 0xEE, 0xFE,
    0x0F, 0x1F, 0x2F, 0x3F, 0x4F, 0x5F, 0x6F, 0x7F, 0x8F, 0x9F, 0xAF, 0xBF, 0xCF, 0xDF, 0xEF, 0xFF };


int sdump (FILE *fBmp)
    {
    int nWrite = fwrite ("BM", 1, 2, fBmp);
    if ( nWrite != 2 ) return 198;
    int nClr = 1 << pmode->ncbt;
    uint8_t pBuff[80];
    int *iBuff = (int *) pBuff;
    iBuff[0] = 54 + 4 * nClr + pmode->grow * pmode->nbpl;   // File size
    iBuff[1] = 0;                                           // Reserved
    iBuff[2] = 54 + 4 * nClr;                               // Offset to pixel data
    iBuff[3] = 40;                                          // BITMAPINFOHEADER size
    iBuff[4] = pmode->gcol;                                 // Width in pixels
    iBuff[5] = pmode->grow;                                 // Height in pixels
    iBuff[6] = ( pmode->ncbt << 16 ) + 1;                   // Number of colour planes (low word) + Number of bits per pixel (high word)
    iBuff[7] = 0;                                           // No compression
    iBuff[8] = 0;                                           // Raw image size (dummy value)
    iBuff[9] = 3000;                                        // Horizontal resolution (pixels / meter)
    iBuff[10] = 3000;                                       // Vertical resolution (pxels / meter)
    iBuff[11] = nClr;                                       // Number of colours
    iBuff[12] = 0;                                          // All colours important
    nWrite = fwrite (iBuff, sizeof (int), 13, fBmp);
    if ( nWrite != 13 ) return 198;
    for (int iClr = 0; iClr < nClr; ++iClr)
        {
        iBuff[iClr] = clrrgb (iClr);
        }
    nWrite = fwrite (iBuff, sizeof (int), nClr, fBmp);
    if ( nWrite != nClr ) return 198;
    const uint8_t *pswap;
    switch (pmode->ncbt)
        {
        case 1: pswap = pmsk02; break;
        case 2: pswap = swap04; break;
        case 4: pswap = swap16; break;
        }
    for (int iRow = pmode->grow - 1; iRow >= 0 ; --iRow)
        {
        uint8_t *fp = framebuf + pmode->nbpl * iRow;
        uint8_t *fpEnd = fp + pmode->nbpl;
        while ( fp < fpEnd )
            {
            for (int i = 0; i < sizeof (pBuff); ++i)
                {
                pBuff[i] = pswap[*fp];
                ++fp;
                }
            nWrite = fwrite (pBuff, 1, sizeof (pBuff), fBmp);
            if ( nWrite != sizeof (pBuff) ) return 198;
            }
        }
    return 0;
    }

int sload (FILE *fBmp)
    {
    uint16_t iBM;
    int nRead = fread (&iBM, sizeof (iBM), 1, fBmp);
    if ((nRead != 1) || (iBM != 0x4D42)) return 255;
    int nClr = 1 << pmode->ncbt;
    uint8_t pBuff[80];
    int *iBuff = (int *) pBuff;
    nRead = fread (iBuff, sizeof (int), 13, fBmp);
    if ((nRead != 13) || (iBuff[3] != 40)) return 255;
    if ((iBuff[4] != pmode->gcol) || (iBuff[5] != pmode->grow)
        || (iBuff[6] != ( pmode->ncbt << 16 ) + 1) || (iBuff[11] != nClr)) return 25;
    int iOff = iBuff[2];
    if (iBuff[2] == 54 + 4 * nClr) iOff = -1;
    nRead = fread (iBuff, sizeof (int), nClr, fBmp);
    if (nRead != nClr) return 255;
    for (int iClr = 0; iClr < nClr; ++iClr)
        {
        clrset (iClr, 16, iBuff[iClr] & 0xFF, (iBuff[iClr] >> 8) & 0xFF, (iBuff[iClr] >> 16) & 0xFF);
        }
    if (iOff > 0) fseek (fBmp, iOff, SEEK_SET);
    const uint8_t *pswap;
    switch (pmode->ncbt)
        {
        case 1: pswap = pmsk02; break;
        case 2: pswap = swap04; break;
        case 4: pswap = swap16; break;
        }
    for (int iRow = pmode->grow - 1; iRow >= 0 ; --iRow)
        {
        uint8_t *fp = framebuf + pmode->nbpl * iRow;
        uint8_t *fpEnd = fp + pmode->nbpl;
        while ( fp < fpEnd )
            {
            nRead = fread (pBuff, 1, sizeof (pBuff), fBmp);
            if (nRead != sizeof (pBuff)) return 255;
            for (int i = 0; i < sizeof (pBuff); ++i)
                {
                *fp = pswap[pBuff[i]];
                ++fp;
                }
            }
        }
    return 0;
    }

void prtscrn (void)
    {
    static int iPrt = 0;
    if ( ++iPrt >= 100 ) iPrt = 1;
#if defined(HAVE_LFS) && defined(HAVE_FAT)
    char sPath[20];
    sprintf (sPath, "sdcard/prtscr%02d.bmp", iPrt);
#else    
    char sPath[13];
    sprintf (sPath, "prtscr%02d.bmp", iPrt);
#endif
    FILE *fBmp = fopen (sPath, "wb");
    sdump (fBmp);
    fclose (fBmp);
    }
