/* fbufvdu.c - Hardware interface for implementing VDU on a framebuffer */

#ifndef H_FBUFVDU
#define H_FBUFVDU

#include <stdint.h>
#include <stdbool.h>

// DBUF_MODE =  0 No double buffer
//              1 One fixed buffer and second buffer above himem
//              2 One fixed buffer and second buffer below PAGE
//              3 Two buffers both below PAGE
#define DBUF_MODE   1

#if HIRES
#define SWIDTH  800
#define SHEIGHT 600
#else
#define SWIDTH  640
#define SHEIGHT 480
#endif
#define BUF_SIZE    (SWIDTH * SHEIGHT / 8)  // Maximum size of a framebuffer

#define CSR_OFF 0x80                    // Cursor turned off
#define CSR_INV 0x40                    // Cursor invalid (off-screen)
#define CSR_CNT 0x3F                    // Cursor hide count bits

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

typedef struct
    {
    uint32_t    ncbt;       // Number of colour bits
    uint32_t    gcol;       // Number of displayed pixel columns
    uint32_t    grow;       // Number of displayed pixel rows
    uint32_t    tcol;       // Number of text columns
    uint32_t    trow;       // Number of text row
    uint32_t    vmgn;       // Number of black lines at top (and bottom)
    uint32_t    hmgn;       // Number of black pixels to left (and to right)
    uint32_t    nppb;       // Number of display pixels per byte
    uint32_t    nbpl;       // Number of bytes per line
    uint32_t    yshf;       // Y scale (number of repeats of each line) = 2 ^ yshf
    uint32_t    thgt;       // Text height
    } MODE;

typedef struct
    {
    uint32_t        nclr;       // Number of colours
    const uint32_t *cpx;        // Colour patterns
    uint32_t        bitsh;      // Shift X coordinate to get bit position
    uint32_t        clrmsk;     // Colour mask
    uint32_t        csrmsk;     // Mask for cursor
    } CLRDEF;

bool setmode (int mode, uint8_t **pfbuf, MODE **ppmd, CLRDEF **ppcd);
void dispmode (void);
void hidecsr (void);
void showcsr (void);
void csrdef (int data2);
uint16_t defclr (int clr);
uint16_t rgbclr (int r, int g, int b);
int clrrgb (uint16_t clr);
void genrb (uint16_t *curpal);
uint8_t *swapbuf (void);
uint8_t *singlebuf (void);
uint8_t *doublebuf (void);
const char *checkbuf (void);
void setup_fbuf (const MODE *pm);
void gsize (uint32_t *pwth, uint32_t *phgt);
int bufsize (void);
void bufswap (uint8_t *fbuf);
void mode7flash (void);

#endif
