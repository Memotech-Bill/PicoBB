/*  fbufctl.c - Framebuffer Control for BBC Basic */

// DEBUG =  0   No diagnostics
//          1   General diagnostics
//          2   Interpolator test
//          4   Double buffering
//          8   Buffer swap
#define DEBUG       0

// DBUF_MODE =  0 No double buffer
//              1 One fixed buffer and second buffer above himem
//              2 One fixed buffer and second buffer below PAGE
//              3 Two buffers both below PAGE
#define DBUF_MODE   1

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/sync.h"
#include "hardware/clocks.h"
#include "fbufvdu.h"
#include "periodic.h"
#include <stdio.h>
#include <string.h>
#include "bbccon.h"

void modechg (char mode);
void error (int iErr, const char *psErr);
void *oshwm (void *addr, int mark);

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

#if DBUF_MODE == 0
static uint8_t  framebuf[BUF_SIZE] __attribute((__aligned__(4)));
#define displaybuf framebuf
#define shadowbuf framebuf
#elif DBUF_MODE <= 2
static uint8_t  fbuffer[BUF_SIZE] __attribute((__aligned__(4)));
static uint8_t  *vbuffer[2] = {fbuffer, NULL};
static uint8_t  *framebuf = fbuffer;
static uint8_t  *displaybuf = fbuffer;
static uint8_t  *shadowbuf = fbuffer;
static uint8_t  *vidtop = fbuffer;
static volatile bool bSwap = false;
#elif DBUF_MODE == 3
static uint8_t  *vbuffer[2] = {NULL, NULL};
static uint8_t  *framebuf = NULL;
static uint8_t  *displaybuf = NULL;
static uint8_t  *shadowbuf = NULL;
static uint8_t  *vidtop = NULL;
static volatile bool bSwap = false;
#endif
#if DBUF_MODE == 1
extern void *himem;
extern void *libase;
extern void *libtop;
#endif

int nCsrHide = 0;			            // Cursor hide count (Bit 7 = Cursor off, Bit 6 = Outside screen)
int csrtop;						        // Top of cursor
int csrhgt;						        // Height of cursor

static const MODE *pmode = NULL;        // Pointer to current mode.
static bool bBlank = true;              // Blank video screen
static bool bCsrVis = false;            // Cursor currently rendered
static uint32_t nFlash = 0;             // Time counter for cursor flash
static critical_section_t cs_csr;       // Critical section controlling cursor flash

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

static void flipcsr (void)
    {
    int xp;
    int yp;
    if ( vflags & HRGFLG )
        {
        xp = lastx;
        yp = lasty;
        if (( xp < gvl ) || ( xp + 7 > gvr ) || ( yp < gvt ) || ( yp + pmode->thgt - 1 > gvb ))
            {
            nCsrHide |= CSR_INV;
            bCsrVis = false;
            return;
            }
        }
    else
        {
        if (( ycsr < 0 ) || ( ycsr >= pmode->trow ) || ( xcsr < 0 ) || ( xcsr >= pmode->tcol ))
            {
            nCsrHide |= CSR_INV;
            bCsrVis = false;
            return;
            }
        xp = 8 * xcsr;
        yp = ycsr * pmode->thgt;
        }
    yp += csrtop;
    int xpc = xp;
    int ypc = yp;
    CLRDEF *cdef = &clrdef[pmode->ncbt];
    uint32_t *fb = (uint32_t *)(shadowbuf + yp * pmode->nbpl);
    xp <<= cdef->bitsh;
    fb += xp >> 5;
    xp &= 0x1F;
    uint32_t msk1 = cdef->csrmsk << xp;
    uint32_t msk2 = cdef->csrmsk >> ( 32 - xp );
    for (int i = 0; i < csrhgt; ++i)
        {
        *fb ^= msk1;
        *(fb + 1) ^= msk2;
        fb += pmode->nbpl / 4;
        ++yp;
        }
    bCsrVis = ! bCsrVis;
    VDU_OUT_INT (framebuf, xpc, ypc, xpc + 8, ypc + csrhgt);
    }

void hidecsr (void)
    {
    ++nCsrHide;
    if ( pmode->ncbt != 3 )
        {
        critical_section_enter_blocking (&cs_csr);
        if ( bCsrVis ) flipcsr ();
        critical_section_exit (&cs_csr);
        }
    }

void showcsr (void)
    {
    if ( ( nCsrHide & CSR_CNT ) > 0 ) --nCsrHide;
    if ( vflags & HRGFLG )
        {
        if (( lastx >= gvl ) && ( lastx + 7 <= gvr )
            && ( lasty >= gvt ) && ( lasty + pmode->thgt - 1 <= gvb ))
            nCsrHide &= ~CSR_INV;
        else
            nCsrHide |= CSR_INV;
        }
    else
        {
        if ( ( ycsr >= tvt ) && ( ycsr <= tvb ) && ( xcsr >= tvl ) && ( xcsr <= tvr ))
            nCsrHide &= ~CSR_INV;
        else
            nCsrHide |= CSR_INV;
        }
    if ( nCsrHide == 0 )
        {
        if ( pmode->ncbt != 3 )
            {
            critical_section_enter_blocking (&cs_csr);
            if ( ! bCsrVis ) flipcsr ();
            critical_section_exit (&cs_csr);
            }
        }
    }

static void flashcsr (void)
    {
    if ( nCsrHide == 0 )
        {
        if ( pmode->ncbt == 3 )
            {
            mode7flash ();
            }
        else
            {
            critical_section_enter_blocking (&cs_csr);
            flipcsr ();
            critical_section_exit (&cs_csr);
            }
        }
    }

void csrdef (int data2)
    {
    uint32_t p1 = data2 & 0xFF;
    uint32_t p2 = ( data2 >> 8 ) & 0xFF;
    uint32_t p3 = ( data2 >> 16 ) & 0xFF;
    if ( p1 == 1 )
        {
        if ( p2 == 0 ) nCsrHide |= CSR_OFF;
        else nCsrHide &= ~CSR_OFF;
        }
    else if ( p1 == 0 )
        {
        if ( p2 == 10 )
            {
            if ( p3 < pmode->thgt )
                {
                csrtop = p3;
                cursa = p3;
                }
            }
        else if ( p2 == 11 )
            {
            if ( p3 < pmode->thgt )
                {
                csrhgt = p3 - csrtop + 1;
                cursb = p3;
                }
            }
        }
    }

static PRD_FUNC pnext = NULL;

static void video_periodic (void)
    {
    if ( ++nFlash >= 5 )
        {
        flashcsr ();
        nFlash = 0;
        }
    if (pnext) pnext ();
    }

void setup_fbuf (const MODE *pm)
    {
    pmode = pm;
    critical_section_init (&cs_csr);
#if DBUF_MODE > 0
    displaybuf = vbuffer[0];
    shadowbuf = vbuffer[0];
#endif
    memset (shadowbuf, 0, BUF_SIZE);
    pnext = add_periodic (video_periodic);
    }

#include <unistd.h>
int usleep(useconds_t usec)
    {
    sleep_us (usec);
    }

void *videobuf (int iBuf, void *pmem)
    {
#if DBUF_MODE >= 2
    pmem = (uint8_t *)(((int)pmem + 3) & 0xFFFFFFFC);
#if DBUF_MODE == 3
    if ( iBuf == 0 )
        {
        vbuffer[0] = (uint8_t *) pmem;
        framebuf = vbuffer[0];
        displaybuf = vbuffer[0];
        shadowbuf = vbuffer[0];
        pmem += BUF_SIZE;
        }
#endif
    if ( iBuf == 1 )
        {
        vbuffer[1] = pmem;
        pmem += BUF_SIZE;
        }
#endif
    return pmem;
    }

#if DBUF_MODE == 0

uint8_t *swapbuf (void)
    {
    return framebuf;
    }

uint8_t *singlebuf (void)
    {
    return framebuf;
    }

uint8_t *doublebuf (void)
    {
    return framebuf;
    }

#else

uint8_t *swapbuf (void)
    {
    if ( shadowbuf != vbuffer[0] )
        {
#if DEBUG & 8
        printf ("swapbuf: vbuffer[0] = %p, displaybuf = %p, shadowbuf = %p\n",
            vbuffer[0], displaybuf, shadowbuf);
#endif
        hidecsr ();
        displaybuf = shadowbuf;
        bufswap (displaybuf);
#if DEBUG & 8
        printf ("Displaying shadowbuf\n");
#endif
        memcpy (vbuffer[0], shadowbuf, bufsize ());
        displaybuf = vbuffer[0];
        bufswap (displaybuf);
#if DEBUG & 8
        printf ("Displaying displaybuf\n");
#endif
        showcsr ();
        }
    return shadowbuf;
    }

uint8_t *singlebuf (void)
    {
#if DEBUG & 4
    printf ("singlebuf\n");
#endif
    if ( shadowbuf != vbuffer[0] ) swapbuf ();
    if ( libtop == (void *)vidtop )
        {
        libtop = vbuffer[1];
        oshwm (libtop, 0);
        }
    shadowbuf = vbuffer[0];
    vidtop = shadowbuf + bufsize ();
    return shadowbuf;
    }

uint8_t *doublebuf (void)
    {
    if ( shadowbuf == vbuffer[0] )
        {
        int nbyt = bufsize ();
        if ( 2 * nbyt <= BUF_SIZE )
            {
#if DEBUG & 4
            printf ("doublebuf: Using top of buffer 0\n");
#endif
            hidecsr ();
            shadowbuf = vbuffer[0] + nbyt;
            }
        else
            {
#if DEBUG & 4
            printf ("doublebuf: Using buffer 1\n");
#endif
#if DBUF_MODE == 1
            vbuffer[1] = (uint8_t *)(((int)himem + 3) & 0xFFFFFFFC);
            int nfree = (uint8_t *)libase - vbuffer[1];
            if ( nfree < nbyt )
                {
                if ( libase > 0 )
                    vbuffer[1] = (uint8_t *)(((int)libtop + 3) & 0xFFFFFFFC);
                nfree = (uint8_t *)(&nbyt) - vbuffer[1] - 0x800;
#if DEBUG & 4
                printf ("doublebuf: nbyt = 0x%04X, nfree = 0x%04X, vbuffer[1] = %p, stack = %p\n",
                    nbyt, nfree, vbuffer[1], &nbyt);
#endif
                if ( nbyt + 0x280 > nfree )
                    {
                    error (255, "No room for refresh buffer");
                    }
                libtop = (void *)(vbuffer[1] + nbyt);
                oshwm (libtop, 0);
                }
#if DEBUG & 4
            else
                {
                printf ("doublebuf: buffer 1 between himem and libase\n");
                }
#endif
#endif
            hidecsr ();
            shadowbuf = vbuffer[1];
            }
        vidtop = shadowbuf + nbyt;
#if DBUF_MODE >= 2
        if ( (void *)vidtop > vpage )
            {
            shadowbuf = vbuffer[0];
            error (255, "No room for refresh buffer");
            }
#endif
        memcpy (shadowbuf, displaybuf, nbyt);
        showcsr ();
        }
    return shadowbuf;
    }

const char *checkbuf (void)
    {
#if STACK_CHECK & 3
#if DBUF_MODE == 1
    static const char sErr2[] = "Stack collided with refresh buffer";
    if ( shadowbuf == vbuffer[1] )
        {
        int nbyt = bufsize ();
        int nfree = (uint8_t *)(&nbyt) - vbuffer[1];
#if DEBUG & 4
        printf ("nbyt = 0x%04X, nfree = 0x%04X\n", nbyt, nfree);
#endif
        if ( nfree < nbyt )
            {
#if DEBUG & 4
            printf ("checkbuf: sErr2\n");
#endif
            return sErr2;
            }
        }
#elif DBUF_MODE > 1
    static const char sErr3[] = "PAGE below refresh buffer";
    if (( vpage != NULL ) && ( vpage < (void *)vidtop ))
        {
#if DEBUG & 4
        printf ("checkbuf: sErr3\n");
#endif
        return sErr3;
        }
#endif
#if DEBUG & 4
    printf ("checkbuf: NULL\n");
#endif
#endif
    return NULL;
    }
#endif
