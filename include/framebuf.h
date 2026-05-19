/* framebuf.h - Framebuffer graphics */

#ifndef FRAMEBUF_H
#define FRAMEBUF_H

#include "vducmd.h"
#include "fbufctl.h"

void fbmode (uint8_t *fb, MODE *pm);
#if SOFT_CSR
void flashcsr (void);
#endif

#endif
