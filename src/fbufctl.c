/*  fbufctl.c - Framebuffer Control for BBC Basic */

// DEBUG =  0   No diagnostics
//          1   General diagnostics
//          2   Interpolator test
//          4   Double buffering
//          8   Buffer swap
#define DEBUG       0

#include <pico.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/clocks.h>
#include "vducmd.h"
#include "fbufctl.h"
#include "periodic.h"
#include <stdio.h>
#include <string.h>
#include "bbccon.h"

void error (int iErr, const char *psErr);
void *oshwm (void *addr, int mark);

#if DBUF_MODE == 0
static uint8_t  framebuf[BUF_SIZE] __attribute((__aligned__(4)));
#define displaybuf  framebuf
#define shadowbuf   framebuf
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
#if SOFT_CSR
static uint32_t nFlash = 0;         // Time counter for cursor flash
critical_section_t cs_csr;          // Critical section controlling cursor flash

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
#endif

void setup_fbuf (void)
    {
#if DBUF_MODE > 0
    displaybuf = vbuffer[0];
    shadowbuf = vbuffer[0];
#endif
    memset (shadowbuf, 0, BUF_SIZE);
#if SOFT_CSR    
    critical_section_init (&cs_csr);
    pnext = add_periodic (video_periodic);
#endif
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
