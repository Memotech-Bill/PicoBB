/*  fbufctl.h - Framebuffer Control for BBC Basic */

#ifndef FBUFCTL_H
#define FBUFCTL_H

// DBUF_MODE =  0 No double buffer
//              1 One fixed buffer and second buffer above himem
//              2 One fixed buffer and second buffer below PAGE
//              3 Two buffers both below PAGE
#ifndef DBUF_MODE
#define DBUF_MODE   1
#endif

#ifndef SWIDTH
#define SWIDTH      640
#endif
#ifndef SHEIGHT
#define SHEIGHT     480
#endif
#ifndef BUF_SIZE
#define BUF_SIZE    (SWIDTH * SHEIGHT / 8)  // Maximum size of a framebuffer
#endif

#if SOFT_CSR
#include <pico/sync.h>
extern critical_section_t cs_csr;       // Critical section controlling cursor flash
#endif

void setup_fbuf (void);
void *videobuf (int iBuf, void *pmem);
uint8_t *swapbuf (void);
uint8_t *singlebuf (void);
uint8_t *doublebuf (void);
#if SOFT_CSR
void flashcsr (void);
#endif

// To be provided by hardware driver
void bufswap (uint8_t *fbuf);
int bufsize (void);

// To be provided by framebuffer renderer
void mode7flash (void);

#endif
