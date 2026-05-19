/*  vducmd.c - Translation of BBC BASIC VDU stream to low level graphics operations */

//  DEBUG =     0   No diagnostic output
//              1   VDU characters
//              2   Principle primitives
//              4   Details
//              8   Filling
#define DEBUG   0

//  VT100_PRT =     0   No VT100 codes to serial port (printer)
//                  1   Send VT100 codes to printer
#define VT100_PRT   0

#define NPLT        3           // Length of plot history
#define BBC_FONT    1           // Use BBC font

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "vducmd.h"
#include "bbccon.h"
#include "lfswrap.h"

#if BBC_FONT == 0
#include "font_10.h"
#endif

// Defined in bbmain.c:
void error (int, const char *);

// Defined in bbpico.c:
extern int getkey (unsigned char *pkey);
extern void bell (void);
// void *gethwm (void);

#if REF_MODE & 2
extern void *himem;
extern void *libase;
extern void *libtop;
static int *vduque = NULL;
static int *vduqbot = NULL;
static int *vduqtop = NULL;
static int nRefQue;
heapptr oshwm (void *addr, int settop);
#endif

// Global variables

int xcsr;                               // Text cursor horizontal position
int ycsr;                               // Text cursor vertical position
int tvt;                                // Top of text viewport
int tvb;                                // Bottom of text viewport
int tvl;                                // Left edge of text viewport
int tvr;                                // Right edge of text viewport
int gvt;                                // Top of graphics viewport
int gvb;                                // Bottom of graphics viewport
int gvl;                                // Left edge of graphics viewport
int gvr;                                // Right edge of graphics viewport

// Local variables

static const MODE *pmode = NULL;        // Current display mode
static uint8_t cmsk;                    // Select colour bits.
#ifdef HAVE_PRINTER
static bool bPrint = false;             // Enable output to printer (UART)
#else
#define bPrint  false
#endif
static int xshift;                      // Shift to convert X graphics units to pixels
static int yshift;                      // Shift to convert Y graphics units to pixels
static bool bFontExp = false;           // Exploded font flag
static uint8_t *fontmap[8] = {          // Pointer to glyph for each block of characters
    bbcfont, bbcfont + 0x100, bbcfont + 0x200, bbcfont + 0x300,
    bbcfont + 0x400, bbcfont + 0x500, bbcfont + 0x600, bbcfont + 0x700};
typedef struct
    {
    int x;
    int y;
    } GPOINT;
static GPOINT pltpt[NPLT];              // History of plotted points (graphics units from top left)


static inline int gxscale (int xp)
    {
    return xp >> xshift;
    }

static inline int gyscale (int yp)
    {
    return pmode->grow - 1 - (yp >> yshift);
    }

static inline uint8_t clrmsk (int clr)
    {
    return clr & cmsk;
    }

static void newpt (int xp, int yp)
    {
#if DEBUG & 2
    printf ("newpt (%d, %d)\n", xp, yp);
#endif
    memmove (&pltpt[1], &pltpt[0], (NPLT - 1) * sizeof (pltpt[0]));
    pltpt[0].x = xp;
    pltpt[0].y = yp;
    prevx = lastx;
    prevy = lasty;
    lastx = pltpt[0].x >> xshift;
    lasty = pltpt[0].y >> yshift;
    }

static void newpix (int xp, int yp)
    {
    memmove (&pltpt[1], &pltpt[0], (NPLT - 1) * sizeof (pltpt[0]));
    pltpt[0].x = xp << xshift;
    pltpt[0].y = yp << yshift;
    lastx = xp;
    lasty = yp;
    }

bool csrpos (int *px, int *py)
    {
    if ( vflags & HRGFLG )
        {
        if (( lastx < gvl ) || ( lastx + 7 > gvr ) || ( lasty < gvt ) || ( lasty + pmode->thgt - 1 > gvb ))
            {
            return false;
            }
        *px = lastx;
        *py = lasty;
        }
    else
        {
        if (( ycsr < 0 ) || ( ycsr >= pmode->trow ) || ( xcsr < 0 ) || ( xcsr >= pmode->tcol ))
            {
            return false;
            }
        *px = 8 * xcsr;
        *py = ycsr * pmode->thgt;
        }
    return true;
    }

static void csrdef (int data2)
    {
    uint32_t p1 = data2 & 0xFF;
    uint32_t p2 = ( data2 >> 8 ) & 0xFF;
    uint32_t p3 = ( data2 >> 16 ) & 0xFF;
    if ( p1 == 1 )
        {
        enablecsr (p2 != 0);
        }
    else if ( p1 == 0 )
        {
        if ( p2 == 10 )
            {
            if ( p3 < pmode->thgt )
                {
                cursa = p3;
                }
            }
        else if ( p2 == 11 )
            {
            if ( p3 < pmode->thgt )
                {
                cursb = p3;
                }
            }
        }
    }

void home (void)
    {
    hidecsr ();
    if ( vflags & HRGFLG )
        {
        newpix (gvl, gvt);
        if ( scroln & 0x80 ) scroln = 0x80 + ( gvb - gvt + 1 ) / pmode->thgt;
        }
    else
        {
        ycsr = tvt;
        xcsr = tvl;
        if ( scroln & 0x80 ) scroln = 0x80 + tvb - tvt + 1;
        }
    showcsr ();
    }

static void dispchr (int chr)
    {
    if (( ycsr < 0 ) || ( ycsr >= pmode->trow ) || ( xcsr < 0 ) || ( xcsr >= pmode->tcol ))
        {
#if DEBUG & 2
        printf ("Cursor out of range: ycsr = %d, xcsr = %d, screen = %dx%d\n",
            ycsr, xcsr, pmode->trow, pmode->tcol);
#endif
        return;
        }
#if DEBUG & 4
    if ((chr >= 0x20) && (chr <= 0x7F)) printf ("dispchr ('%c')\n", chr);
    else printf ("dispchr (0x%02X)\n", chr);
#endif
    if ( pmode->ncbt == 3 )
        {
        disp_ttx (chr & 0x7F);
        }
    else
        {
        hidecsr ();
        disp_glyph (&fontmap[chr >> 5][8 * (chr & 0x1F)]);
        showcsr ();
        }
    }

static void newline (int *px, int *py)
    {
    hidecsr ();
    if (++(*py) > tvb)
        {
        if ((scroln & 0x80) && (--scroln == 0x7F))
            {
            unsigned char ch;
            scroln = 0x80 + tvb - tvt + 1;
            do
                {
                usleep (5000);
                } 
            while ((getkey (&ch) == 0) && ((flags & (ESCFLG | KILL)) == 0));
            }
        scrlup ();
        *py = tvb;
        }
	if ( bPrint )
        {
        printf ("\n");
#if VT100_PRT
        if (*px) printf ("\033[%i;%iH", *py + 1, *px + 1);
#endif
        }
    showcsr ();
    }

static void wrap (void)
    {
    if ( bPrint ) printf ("\r");
    hidecsr ();
    xcsr = tvl;
    newline (&xcsr, &ycsr);
    showcsr ();
    }

static void tabxy (int x, int y)
    {
#if DEBUG & 2
    printf ("tab: ycsr = %d, xcsr = %d\n", y, x);
#endif
    x += tvl;
    y += tvt;
    if (( x >= tvl ) && ( x <= tvr ) && ( y >= tvt ) && ( y <= tvb ))
        {
        hidecsr ();
        ycsr = y;
        xcsr = x;
        showcsr ();
        }
    }

static void twind (int vl, int vb, int vr, int vt)
    {
    if (( vl < 0 ) || ( vl > vr ) || ( vr >= pmode->tcol )
        || ( vt < 0 ) || ( vt > vb ) || ( vb >= pmode->trow )) return;
    tvl = vl;
    tvr = vr;
    tvt = vt;
    tvb = vb;
    textwl = 8 * tvl;
    textwr = 8 * tvr + 7;
    textwt = pmode->thgt * tvt;
    textwb = pmode->thgt * (tvb + 1) - 1;
    if (( xcsr < tvl ) || ( xcsr > tvr ) || ( ycsr < tvt ) || ( ycsr > tvb ))
        home ();
    }

static void rstview (void)
    {
    hidecsr ();
    twind (0, pmode->trow - 1, pmode->tcol - 1, 0);
    xcsr = 0;
    ycsr = 0;
    origx = 0;
    origy = 0;
    gvt = 0;
    gvb = pmode->grow - 1;
    gvl = 0;
    gvr = pmode->gcol - 1;
    newpt (0, (pmode->grow << yshift) - 1);
    showcsr ();
    }

void modechg (int mode)
    {
#if DEBUG & 2
    printf ("modechg (%d)\n", mode);
#endif
    hidecsr ();
    const MODE *pnm;
    if ( pnm = setmode (mode) )
        {
        pmode = pnm;
        uint32_t gwth;
        uint32_t ghgt;
        gsize (&gwth, &ghgt);
#if DEBUG & 2
        printf ("grow = %d, gcol = %d\n", pmode->grow, pmode->gcol);
#endif
        modeno = mode;
        int gunit = pmode->gcol;
        pixelx = 1;
        xshift = 0;
        while ( gunit <= gwth )
            {
            ++xshift;
            pixelx <<= 1;
            gunit <<= 1;
            }
        gunit = pmode->grow;
        pixely = 1;
        yshift = 0;
        while ( gunit <= ghgt )
            {
            ++yshift;
            pixely <<= 1;
            gunit <<= 1;
            }
#if DEBUG & 2
        printf ("pixelx = %d, xshift = %d, pixely = %d, yshift = %d\n", pixelx, xshift, pixely, yshift);
#endif
        cmsk = (1 << pmode->ncbt) - 1;
        txtbak = 0;
        bakgnd = 0;
        txtfor = cmsk;
        forgnd = txtfor;
        clrreset ();
        rstview ();
        cls ();
        cursa = pmode->thgt - 1;
        cursb  = cursa;
        bPaletted = 1;
        sizex = pmode->gcol;
        sizey = pmode->grow;
        charx = 8;
        chary = pmode->thgt;
#if DEBUG & 2
        printf ("modechg: Completed cls\n");
#endif
        dispenable ();
#if REF_MODE > 0
        if ( reflag == 1 ) refresh_off ();
#endif
#if DEBUG & 2
        printf ("modechg: New mode set\n");
#endif
        showcsr ();
        }
    else
        {
        showcsr ();
        error (25, NULL);
        }
    }

static void clippoint  (int clrop, int xp, int yp)
    {
    if (( xp >= gvl ) && ( xp <= gvr ) && ( yp >= gvt ) && ( yp <= gvb )) point (clrop, xp, yp);
    }

#define SKIP_NONE   0
#define SKIP_FIRST  -1
#define SKIP_LAST   1

static void line (int clrop, uint32_t xp1, uint32_t yp1, uint32_t xp2, uint32_t yp2, uint32_t dots, int skip)
    {
#if DEBUG & 2
    printf ("line (0x%02X, %d, %d, %d, %d, 0x%08X, %d)\n", clrop, xp1, yp1, xp2, yp2, dots, skip);
#endif
    int xd = xp2 - xp1;
    int yd = yp2 - yp1;
    bool bVert = false;
    bool bSwap = false;
    if ( xd >= 0 )
        {
        if ( yd > xd )
            {
            bVert = true;
            }
        else if ( yd < -xd )
            {
            bVert = true;
            bSwap = true;
            }
        }
    else
        {
        bSwap = true;
        if ( yd > -xd )
            {
            bVert = true;
            bSwap = false;
            }
        else if ( yd < xd )
            {
            bVert = true;
            }
        }
    if ( bSwap )
        {
        uint32_t tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        xd  = -xd;
        yd  = -yd;
        skip = -skip;
        }
    if ( bVert )
        {
#if DEBUG & 2
    printf ("vertical: %d, %d, %d, %d\n", xp1, yp1, xp2, yp2);
#endif
        int xs = 1;
        if ( xd < 0 )
            {
            xs = -1;
            xd = -xd;
            }
        int xa = 0;
        if ( skip == SKIP_FIRST )
            {
            if ( dots & 1 )
                {
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            xa += xd;
            if ( xa >= yd )
                {
                xp1 +=  xs;
                xa -= yd;
                }
            ++yp1;
            }
        else if ( skip == SKIP_LAST )
            {
            --yp2;
            }
        while ( yp1 <= yp2 )
            {
            if ( dots & 1 )
                {
                point (clrop, xp1, yp1);
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            xa += xd;
            if ( xa >= yd )
                {
                xp1 +=  xs;
                xa -= yd;
                }
            ++yp1;
            }
        }
    else
        {
#if DEBUG & 2
    printf ("horizontal: %d, %d, %d, %d\n", xp1, yp1, xp2, yp2);
#endif
        int ys = 1;
        if ( yd < 0 )
            {
            ys = -1;
            yd = -yd;
            }
        int ya = 0;
        if ( skip == SKIP_FIRST )
            {
            if ( dots & 1 )
                {
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            ya += yd;
            if ( ya >= xd )
                {
                yp1 += ys;
                ya -= xd;
                }
            ++xp1;
            }
        else if ( skip == SKIP_LAST )
            {
            --xp2;
            }
        while ( xp1 <= xp2 )
            {
            if ( dots & 1 )
                {
                point (clrop, xp1, yp1);
                dots >>= 1;
                dots |= 0x80000000;
                }
            else
                {
                dots >>= 1;
                }
            ya += yd;
            if ( ya >= xd )
                {
                yp1 += ys;
                ya -= xd;
                }
            ++xp1;
            }
        }
    }

static void clipline (int clrop, int xp1, int yp1, int xp2, int yp2, uint32_t dots, int skip)
    {
#if DEBUG & 2
    printf ("clipline (0x%02X, %d, %d, %d, %d, 0x%08X, %d)\n", clrop, xp1, yp1, xp2, yp2, dots, skip);
    printf ("clip limits: %d %d %d %d\n", gvl, gvt, gvr, gvb);
#endif
    if ( xp2 < xp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        skip = -skip;
        }
    if (( xp2 < gvl ) || ( xp1 > gvr )) return;
    if ( xp1 < gvl )
        {
#if DEBUG & 2
        printf ("Clip left.\n");
#endif
        yp1 += ( yp2 - yp1 ) * ( gvl - xp1 ) / ( xp2 - xp1 );
        xp1 = gvl;
        if ( skip == SKIP_FIRST ) skip = SKIP_NONE;
        }
    if ( xp2 > gvr )
        {
#if DEBUG & 2
        printf ("Clip right.\n");
#endif
        yp2 -= ( yp2 - yp1 ) * ( xp2 - gvr ) / ( xp2 - xp1 );
        xp2 = gvr;
        if ( skip == SKIP_LAST ) skip = SKIP_NONE;
        }
    if ( yp2 < yp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        skip = -skip;
        }
    if (( yp2 < gvt ) || ( yp1 > gvb )) return;
    if ( yp1 < gvt )
        {
#if DEBUG & 2
        printf ("Clip top.\n");
#endif
        xp1 += ( xp2 - xp1 ) * ( gvt - yp1 ) / ( yp2 - yp1 );
        yp1 = gvt;
        if ( skip == SKIP_FIRST ) skip = SKIP_NONE;
        }
    if ( yp2 > gvb )
        {
#if DEBUG & 2
        printf ("Clip bottom.\n");
#endif
        xp2 -= ( xp2 - xp1 ) * ( yp2 - gvb ) / ( yp2 - yp1 );
        yp2 = gvb;
        if ( skip == SKIP_LAST ) skip = SKIP_NONE;
        }
    line (clrop, xp1, yp1, xp2, yp2, dots, skip);
    }

static void hdraw (int clrop, int xp1, int xp2, int yp)
    {
    if (( yp < gvt ) || ( yp > gvb )) return;
    if ( xp1 > xp2 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        }
    if (( xp2 < gvl ) || ( xp1 > gvr )) return;
    if ( xp1 < gvl ) xp1 = gvl;
    if ( xp2 > gvr ) xp2 = gvr;
    hline (clrop, xp1, xp2, yp);
    }

static void triangle (int clrop, int xp1, int yp1, int xp2, int yp2, int xp3, int yp3)
    {
#if DEBUG & 2
    printf ("triangle (0x%02X, (%d, %d), (%d, %d), (%d, %d))\n", clrop, xp1, yp1, xp2, yp2, xp3, yp3);
#endif
    if (( xp1 < gvl ) && ( xp2 < gvl ) && ( xp3 < gvl )) return;
    if (( xp1 > gvr ) && ( xp2 > gvr ) && ( xp3 > gvr )) return;
    if (( yp1 < gvt ) && ( yp2 < gvt ) && ( yp3 < gvt )) return;
    if (( yp1 > gvb ) && ( yp2 > gvb ) && ( yp3 > gvb )) return;
    if ( yp2 < yp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        }
    if ( yp3 < yp2 )
        {
        int tmp = xp2;
        xp2 = xp3;
        xp3 = tmp;
        tmp = yp2;
        yp2 = yp3;
        yp3 = tmp;
        }
    if ( yp2 < yp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        }
    int aa = 0;
    int xa = xp1;
    int na = xp2 - xp1;
    int da = yp2 - yp1;
    int ab = 0;
    int xb = xp1;
    int nb = xp3 - xp1;
    int db = yp3 - yp1;
    while ( yp1 < yp2 )
        {
        hdraw (clrop, xa, xb, yp1);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yp1;
        }
    aa = 0;
    xa = xp2;
    na = xp3 - xp2;
    da = yp3 - yp2;
    while ( yp1 < yp3 )
        {
        hdraw (clrop, xa, xb, yp1);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yp1;
        }
    hdraw (clrop, xa, xb, yp1);
    }

static void rectangle (int clrop, int xp1, int yp1, int xp2, int yp2)
    {
#if DEBUG & 2
    printf ("rectangle (0x%02X, (%d, %d), (%d, %d))\n", clrop, xp1, yp1, xp2, yp2);
#endif
    if ( xp2 < xp1 )
        {
        int tmp = xp1;
        xp1 = xp2;
        xp2 = tmp;
        }
    if ( yp2 < yp1 )
        {
        int tmp = yp1;
        yp1 = yp2;
        yp2 = tmp;
        }
    if (( xp2 < gvl ) || ( xp1 > gvr ) || ( yp2 < gvt ) || ( yp1 > gvb )) return;
    while ( yp1 <= yp2 )
        {
        hdraw (clrop, xp1, xp2, yp1);
        ++yp1;
        }
    }

static void trapizoid (int clrop, int xp1, int yp1, int xp2, int yp2, int xp3, int yp3)
    {
#if DEBUG & 2
    printf ("trapizoid (0x%02X, (%d, %d), (%d, %d), (%d, %d))\n", clrop, xp1, yp1, xp2, yp2, xp3, yp3);
#endif
    GPOINT pts[4] = {{xp1, yp1}, {xp2, yp2}, {xp3, yp3}, {xp1 - xp2 + xp3, yp1 - yp2 + yp3}};
    int xmin = xp1;
    int xmax = xp1;
    int ymin = yp1;
    int ymax = yp1;
    int itop = 0;
    for (int i = 1; i < 4; ++i )
        {
        if ( pts[i].x < xmin ) xmin = pts[i].x;
        if ( pts[i].x > xmax ) xmax = pts[i].x;
        if ( pts[i].y < ymin )
            {
            ymin = pts[i].y;
            itop = i;
            }
        if ( pts[i].y > ymax ) ymax = pts[i].y;
        }
    if (( xmax < gvl ) || ( xmin > gvr ) || ( ymax < gvt ) || ( ymin > gvb )) return;
    GPOINT *pp[4];
    for (int i = 0; i < 4; ++i)
        {
        pp[i] = &pts[itop];
        if ( ++itop > 3 ) itop = 0;
        }
    if ( pp[1]->y > pp[3]->y )
        {
        GPOINT *pt = pp[1];
        pp[1] = pp[3];
        pp[3] = pt;
        }
    int yy = pp[0]->y;
    int aa = 0;
    int xa = pp[0]->x;
    int na = pp[1]->x - xa;
    int da = pp[1]->y - yy;
    int ab = 0;
    int xb = xa;
    int nb = pp[3]->x - xb;
    int db = pp[3]->y - yy;
    while ( yy < pp[1]->y )
        {
        hdraw (clrop, xa, xb, yy);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yy;
        }
    aa = 0;
    xa = pp[1]->x;
    na = pp[2]->x - xa;
    da = pp[2]->y - yy;
    while ( yy < pp[3]->y )
        {
        hdraw (clrop, xa, xb, yy);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yy;
        }
    ab = 0;
    xb = pp[3]->x;
    nb = pp[2]->x - xb;
    db = pp[2]->y - yy;
    while ( yy < pp[2]->y )
        {
        hdraw (clrop, xa, xb, yy);
        aa += na;
        xa += aa / da;
        aa %= da;
        ab += nb;
        xb += ab / db;
        ab %= db;
        ++yy;
        }
    hdraw (clrop, xa, xb, yy);
    }

static void gwind (int vl, int vb, int vr, int vt)
    {
    vl = gxscale (vl);
    vr = gxscale (vr);
    vt = gyscale (vt);
    vb = gyscale (vb);
    if (( vl < 0 ) || ( vl > vr ) || ( vr >= pmode->gcol )
        || ( vt < 0 ) || ( vt > vb ) || ( vb >= pmode->grow )) return;
    gvl = vl;
    gvr = vr;
    gvt = vt;
    gvb = vb;
    }

// Get text cursor (caret) coordinates:
void getcsr(int *px, int *py)
    {
    if ( px ) *px = xcsr - tvl;
    if ( py ) *py = ycsr - tvt;
    }

int vpoint (int xp, int yp)
    {
    xp = gxscale (xp);
    yp = gyscale (yp);
    if (( xp < gvl ) || ( xp > gvr ) || ( yp < gvt ) || ( yp > gvb )) return -1;
    return getpix (xp, yp);
    }

int vtint (int xp, int yp)
    {
    int clr = vpoint (xp, yp);
    if ( clr < 0 ) return -1;
    return clrrgb (clr);
    }

int vgetc (int x, int y)
    {
    if (( x == 0x80000000 ) && ( y == 0x80000000 ))
        {
        x = xcsr;
        y = ycsr;
        }
    else
        {
        x += tvl;
        y += tvt;
        if (( x < tvl ) || ( x > tvr ) || ( y < tvt ) || ( y > tvb )) return -1;
        }
    if ( pmode->ncbt == 3 )
        {
        int chr = get_ttx (x, y);
        return chr;
        }
    else
        {
        uint8_t chrow[10];
        int bg = ( vflags & HRGFLG ) ? bakgnd : txtbak;
        get_glyph (x, y, bg, chrow);
        int fhgt = 8;
        for (int i = 0; i < 256; ++i)
            {
            bool bMatch = true;
            for (int j = 0; j < fhgt; ++j)
                {
                if ( chrow[j] != fontmap[i >> 5][8 * (i & 0x1F) + j] )
                    {
                    bMatch = false;
                    break;
                    }
                }
            if ( bMatch ) return i;
            }
        }
    return -1;
    }

// Get string width in graphics units:
int widths (unsigned char *s, int l)
    {
	return ( 8 * l ) << xshift;
    }

#define FT_LEFT     0x01
#define FT_RIGHT    0x02
#define FT_EQUAL    0x04

static void linefill (int clrop, int xp, int yp, int ft, uint8_t clr)
    {
#if DEBUG & 2
    printf ("linefill (0x%02X, %d, %d, %d, 0x%02X)\n", clrop, xp,yp, ft, clr);
#endif
    if (( xp < gvl ) || ( xp > gvr ) || ( yp < gvt ) || ( yp > gvb )) return;
    int xp1 = xp;
    if ( ft & FT_LEFT )
        {
        while ( xp1 > gvl )
            {
            uint8_t pix = getpix (xp1 - 1, yp);
            if ( pix == clr )
                {
                if ( ft & FT_EQUAL ) break;
                }
            else
                if ( !( ft & FT_EQUAL ) ) break;
            --xp1;
            }
        }
    int xp2 = xp;
    if ( ft & FT_RIGHT )
        {
        while ( xp2 < gvr )
            {
            uint8_t pix = getpix (xp2 + 1, yp);
            if ( pix == clr )
                {
                if ( ft & FT_EQUAL ) break;
                }
            else
                if ( !( ft & FT_EQUAL ) ) break;
            ++xp2;
            }
        }
    hdraw (clrop, xp1, xp2, yp);
    }

typedef struct
    {
    int     clrop;
    bool    bEq;
    uint8_t tclr;
    } FILLINFO;

static inline bool doflood (FILLINFO *pfi, int xp, int yp)
    {
    if (( xp < gvl ) || ( xp > gvr ) || ( yp < gvt ) || ( yp > gvb )) return false;
    uint8_t pclr = getpix (xp, yp);
    bool bRes = ( pclr == pfi->tclr );
    if ( ! pfi->bEq ) bRes = ! bRes;
    if ( bRes )
        {
        uint8_t fclr = clrmsk (pfi->clrop);
        switch (pfi->clrop >> 8)
            {
            case 1:
                if ( ( pclr & fclr ) == fclr ) bRes = false;
                break;
            case 2:
                if ( ( pclr & (~ fclr) ) == 0 ) bRes = false;
                break;
            default:
                if ( pclr == fclr ) bRes = false;
                break;
            }
        }
#if DEBUG & 8
    printf ("doflood (%d, %d) pclr = 0x%02X, bRes = %s\n", xp, yp, pclr, bRes ? "True": "False");
#endif
    return bRes;
    }

#if 1
// Fill a region without overflowing stack. Based upon:
// "Space-efficient Region Filling in Raster Graphics", Dominik Henrich
// "The Visual Computer: An International Journal of Computer Graphics", vol 10, no 4, pp 205-215, 1994.

// Number of additional cursors. In theory non zero values should speed complex fills.
#define MFILLCSR    0

static void FCurMove (FILLINFO *pfi, int iDir, int *pX, int *pY, int *pW)
    {
    int iX = *pX;
    int iY = *pY;
    int iW = *pW;
#if DEBUG & 8
    printf ("FCurMove iX = %d, iY = %d, iDir = %d, iW = 0x%03X\n", iX, iY, iDir, iW);
#endif
    switch (iDir)
        {
        case 0:
            ++iX;
            *pX = iX;
            ++iX;
            iW &= 0x1B6;
            iW >>= 1;
            if ( ! doflood (pfi, iX, iY-1) ) iW |= 0x004;
            if ( ! doflood (pfi, iX, iY) )   iW |= 0x020;
            if ( ! doflood (pfi, iX, iY+1) ) iW |= 0x100;
            break;
        case 1:
            ++iY;
            *pY = iY;
            ++iY;
            iW &= 0x1F8;
            iW >>= 3;
            if ( ! doflood (pfi, iX-1, iY) ) iW |= 0x040;
            if ( ! doflood (pfi, iX, iY) )   iW |= 0x080;
            if ( ! doflood (pfi, iX+1, iY) ) iW |= 0x100;
            break;
        case 2:
            --iX;
            *pX = iX;
            --iX;
            iW &= 0x0DB;
            iW <<= 1;
            if ( ! doflood (pfi, iX, iY-1) ) iW |= 0x001;
            if ( ! doflood (pfi, iX, iY) )   iW |= 0x008;
            if ( ! doflood (pfi, iX, iY+1) ) iW |= 0x040;
            break;
        case 3:
            --iY;
            *pY = iY;
            --iY;
            iW &= 0x03F;
            iW <<= 3;
            if ( ! doflood (pfi, iX-1, iY) ) iW |= 0x001;
            if ( ! doflood (pfi, iX, iY) )   iW |= 0x002;
            if ( ! doflood (pfi, iX+1, iY) ) iW |= 0x004;
            break;
        }
    *pW = iW;
#if DEBUG & 8
    printf ("   iX = %d, iY = %d, iW = 0x%03X\n", *pX, *pY, iW);
#endif
    }

static const int iDirMask[4] = { 0x020, 0x080, 0x008, 0x002};

static int FCurRight (int iDir, int iW)
    {
#if DEBUG & 8
    printf ("FCurRight (%d, 0x%03X)", iDir, iW);
#endif
    if ( ( iW & 0x0AA ) == 0x0AA ) error (255, "Fill stuck");
    iDir = ( iDir + 1 ) & 3;
    while ( iW & iDirMask[iDir] ) iDir = ( iDir + 3 ) & 3;
#if DEBUG & 8
    printf (" = %d\n", iDir);
#endif
    return iDir;
    }

static int FCurLeft (int iDir, int iW)
    {
#if DEBUG & 8
    printf ("FCurLeft (%d, 0x%03X)", iDir, iW);
#endif
    if ( ( iW & 0x0AA ) == 0x0AA ) error (255, "Fill stuck");
    iDir = ( iDir + 3 ) & 3;
    while ( iW & iDirMask[iDir] ) iDir = ( iDir + 1 ) & 3;
#if DEBUG & 8
    printf (" = %d\n", iDir);
#endif
    return iDir;
    }

static int FCurSet (FILLINFO *pfi, int iX, int iY, int iW)
    {
#if DEBUG & 8
    printf ("FCurSet (%d, %d)\n", iX, iY);
#endif
    point (pfi->clrop, iX, iY);
    return iW | 0x010;
    }

static uint16_t iCritMap[] = {
    0x3020,	// 0x000
    0x3322,	// 0x020
    0x30FE,	// 0x040
    0x33FF,	// 0x060
    0x30FE,	// 0x080
    0x0022,	// 0x0A0
    0x30FE,	// 0x0C0
    0x0022,	// 0x0E0
    0xFFFE,	// 0x100
    0x3322,	// 0x120
    0xFFFF,	// 0x140
    0x33FF,	// 0x160
    0x30FE,	// 0x180
    0x0022,	// 0x1A0
    0x30FE,	// 0x1C0
    0x0022,	// 0x1E0
};

static bool FCurCrit (int iW)
    {
    uint16_t map = iCritMap[iW >> 5];
    uint16_t bit = 1 << ( iW & 0x0F );
    bool bCrit = (( map & bit ) != 0 );
#if DEBUG & 8
    printf ("FCurCrit (0x%03X) = 0x%04X, 0x%04X, %s\n", iW, map, bit, bCrit ? "Yes" : "No");
#endif
    return bCrit;
    }

static bool LeftCycle (FILLINFO *pfi, int iXC, int iYC, int iDC, int iW)
    {
#if DEBUG & 8
    printf ("LeftCycle (%d, %d, %d, 0x%03X)\n", iXC, iYC, iDC, iW);
#endif
    iDC = FCurLeft (iDC, iW);
    int iX = iXC;
    int iY = iYC;
    int iDir = iDC;
    FCurMove (pfi, iDir, &iX, &iY, &iW);
    while (( iX != iXC ) || ( iY != iYC ))
        {
        if ( ! FCurCrit (iW) ) iW = FCurSet (pfi, iX, iY, iW);
        iDir = FCurLeft (iDir, iW);
        FCurMove (pfi, iDir, &iX, &iY, &iW);
        }
    iDir = FCurLeft (iDir, iW);
#if DEBUG & 8
    printf ("Completed LeftCycle: iDir = %d, iDC = %d, %s\n", iDir, iDC, (iDir == iDC ) ? "True" : "False");
#endif
    return ( iDir == iDC );
    }

static void FillInit (FILLINFO *pfi, int *pX, int *pY, int *pW)
    {
    int iX = *pX;
    int iY = *pY;
#if DEBUG & 8
    printf ("FillInit (%d, %d)\n", iX, iY);
#endif
    while ( doflood (pfi, iX+1, iY) ) ++iX;
#if DEBUG & 8
    printf ("Found boundary: iX = %d\n", iX);
#endif
    *pX = iX;
    int iW = 0x020;
    if ( ! doflood (pfi, iX-1, iY-1) ) iW |= 0x001;
    if ( ! doflood (pfi, iX,   iY-1) ) iW |= 0x002;
    if ( ! doflood (pfi, iX+1, iY-1) ) iW |= 0x004;
    if ( ! doflood (pfi, iX-1, iY) )   iW |= 0x008;
    if ( ! doflood (pfi, iX-1, iY+1) ) iW |= 0x040;
    if ( ! doflood (pfi, iX,   iY+1) ) iW |= 0x080;
    if ( ! doflood (pfi, iX+1, iY+1) ) iW |= 0x100;
    *pW = iW;
#if DEBUG & 8
    printf ("Completed FillInit: iX = %d, iY = %d, iW = 0x%03X\n", iX, iY, iW);
#endif
    }

#if MFILLCSR > 0
static int nfcur = 0;

static struct
    {
    int iX;
    int iY;
    int iDir;
    } fcurstk[MFILLCSR];

static void FCurPush (int iX, int iY, int iDir)
    {
    fcurstk[nfcur].iX = iX;
    fcurstk[nfcur].iY = iY;
    fcurstk[nfcur].iDir = iDir;
    ++nfcur;
    }

static bool FCurPop (FILLINFO *pfi, int *pX, int *pY, int *pDir, int *pW)
    {
    while ( nfcur > 0 )
        {
        --nfcur;
        int iX = fcurstk[nfcur].iX;
        int iY = fcurstk[nfcur].iY;
        int iDir = fcurstk[nfcur].iDir;
        int iW = 0;
        if ( ! doflood (pfi, iX-1, iY-1) ) iW |= 0x001;
        if ( ! doflood (pfi, iX  , iY-1) ) iW |= 0x002;
        if ( ! doflood (pfi, iX+1, iY-1) ) iW |= 0x004;
        if ( ! doflood (pfi, iX-1, iY  ) ) iW |= 0x008;
        if ( ! doflood (pfi, iX  , iY  ) ) iW |= 0x010;
        if ( ! doflood (pfi, iX+1, iY  ) ) iW |= 0x020;
        if ( ! doflood (pfi, iX-1, iY+1) ) iW |= 0x040;
        if ( ! doflood (pfi, iX  , iY+1) ) iW |= 0x080;
        if ( ! doflood (pfi, iX+1, iY+1) ) iW |= 0x100;
        if ( ( iW & 0x1EF ) != 0x1EF )
            {
            *pX = iX;
            *pY = iY;
            *pDir = iDir;
            *pW = iW;
            return true;
            }
        if ( iW == 0x1EF ) point (pf->clrop, iX, iY);
        }
    return false;
    }
#endif

static void flood (bool bEq, uint8_t tclr, int clrop, int iX, int iY)
    {
#if DEBUG & 2
    printf ("flood (%d, 0x%02X, 0x%02X, %d, %d)\n", bEq, tclr, clrop, iX, iY);
#endif
    FILLINFO fi;
    fi.clrop = clrop;
    fi.bEq = bEq;
    fi.tclr = tclr;
    int iDir = 0;
    int iW;
    FillInit (&fi, &iX, &iY, &iW);
    while (true)
        {
        if ( ( iW & 0x1EF ) == 0x1EF )
            {
            iW = FCurSet (&fi, iX, iY, iW);
#if MFILLCSR > 0
            if ( ! FCurPop (&fi, &iX, &iY, &iDir, &iW) ) return;
#else
            return;
#endif
            }
        if ( FCurCrit (iW) )
            {
#if MFILLCSR > 0
            if ( nfcur <= MFILLCSR )
                {
                FCurPush (iX, iY, iDir);
                iW |= 0x010;
                }
            else if ( LeftCycle (&fi, iX, iY, iDir, iW) ) iW |= 0x010;
#else
            if ( LeftCycle (&fi, iX, iY, iDir, iW) ) iW |= 0x010;
#endif
            }
        else
            {
            iW = FCurSet (&fi, iX, iY, iW);
            }
        iDir = FCurRight (iDir, iW);
        FCurMove (&fi, iDir, &iX, &iY, &iW);
        }
    }

#else
// Traditional recursive fill. Fast but has excessive stack use for complex fills.
static void flood2 (FILLINFO *pfi, int xp, int yp)
    {
#if DEBUG & 2
    printf ("flood2 (%d, %d)\n", xp, yp);
#endif
    if ( ! doflood (&fi, xp, yp) ) return;
    int xl = xp;
    int xr = xp;
    while ( doflood (&fi, xl-1, yp) ) --xl;
    while ( doflood (&fi, xr+1, yp) ) ++xr;
    hdraw (clrop, xl, xr, yp);
    int xc = ( xl + xr ) / 2;
    int yt = yp;
    while ( doflood (&fi, xc, yt-1) ) --yt;
    if ( yt < yp - 1 ) flood2 (pfi, xc, (yp + yt) / 2);
    int yb = yp;
    while ( doflood (&fi, xc, yb+1) ) ++yb;
    if ( yb > yp + 1 ) flood2 (pfi, xc, (yp + yb) / 2);
    for (int xx = xl; xx <= xr; ++xx)
        {
        if ( doflood (pfi, xx, yp-1) ) flood2 (pfi, xx, yp-1);
        if ( doflood (pfi, xx, yp+1) ) flood2 (pfi, xx, yp+1);
        }
    }

static void flood (bool bEq, uint8_t tclr, int clrop, int xp, int yp)
    {
#if DEBUG & 2
    printf ("flood (%d, 0x%02X, 0x%02X, %d, %d)\n", bEq, tclr, clrop, xp, yp);
#endif
    FILLINFO fi;
    fi.clrop = clrop;
    fi.bEq = bEq;
    fi.tclr = tclr;
    flood2 (&fi, xp, yp);
    }
#endif

static int iroot (int s)
    {
#if DEBUG & 2
    printf ("iroot (%d)\n", s);
#endif
    int r1 = 0;
    int r2 = s;
    // 46340 = sqrt (MAX_INT)
    if ( r2 > 46340 ) r2 = 46340;
    while ( r2 > r1 + 1 )
        {
        int r3 = ( r1 + r2 ) / 2;
        int st = r3 * r3;
#if DEBUG & 4
        printf ("r1 = %d, r2 = %d, r3 = %d, st = %d\n", r1, r2, r3, st);
#endif
        if ( st == s ) { r1 = r3; break; }
        else if ( st < s ) r1 = r3;
        else r2 = r3;
        }
#if DEBUG & 2
    printf ("iroot (%d) = %d\n", s, r1);
#endif
    return r1;
    }

static void ellipse (bool bFill, int clrop, int xc, int yc, uint32_t aa, uint32_t bb, uint64_t dd)
    {
#if DEBUG & 2
    printf ("ellipse (%d, 0x%02X, (%d, %d), %d, %d, %lld)\n", bFill, clrop, xc, yc, aa, bb, dd);
#endif
    uint32_t xp = pmode->gcol;
    uint32_t yp = 0;
    uint32_t xl = 0;
    int dx = 0;
    while ( true )
        {
        uint32_t sq = yp * yp;
        uint64_t pp = ((uint64_t) bb) * ((uint64_t) sq);
        if ( pp > dd ) break;
        uint64_t qq = dd - pp;
        sq = xp * xp;
        pp = ((uint64_t) aa) * ((uint64_t) sq);
        if ( pp > qq )
            {
            int x1 = 0;
            int x2 = xp;
            while ( x2 - x1 > 1 )
                {
                if ( dx > 0 )
                    {
                    xp = x2 - dx;
                    dx *= 2;
                    if ( xp < x1 )
                        {
                        xp = ( x1 + x2 ) / 2;
                        dx = 0;
                        }
                    }
                else
                    {
                    xp = ( x1 + x2 ) / 2;
                    }
                sq = xp * xp;
                pp = ((uint64_t) aa) * ((uint64_t) sq);
                if ( pp > qq )
                    {
                    x2 = xp;
                    }
                else if ( pp == qq )
                    {
                    x1 = xp;
                    break;
                    }
                else
                    {
                    x1 = xp;
                    dx = 0;
                    }
                }
            xp = x1;
            }
        if ( yp == 0 )
            {
            if ( bFill )
                {
                hdraw (clrop, xc - xp, xc + xp, yc + yp);
                }
            dx = 1;
            }
        else if ( bFill )
            {
#if DEBUG & 4
            printf ("yp = %d: fill %d\n", yp, xp);
#endif
            hdraw (clrop, xc - xp, xc + xp, yc + yp);
            hdraw (clrop, xc - xp, xc + xp, yc - yp);
            }
        else if ( xp == xl )
            {
#if DEBUG & 4
            printf ("yp = %d: dot %d\n", yp, xp);
#endif
            clippoint (clrop, xc + xl, yc + yp - 1);
            clippoint (clrop, xc - xl, yc + yp - 1);
            if ( yp > 1 )
                {
                clippoint (clrop, xc + xl, yc - yp + 1);
                clippoint (clrop, xc - xl, yc - yp + 1);
                }
            }
        else
            {
#if DEBUG & 4
            printf ("yp = %d: dash %d - %d\n", yp, xp, xl);
#endif
            hdraw (clrop, xc + xp + 1, xc + xl, yc + yp - 1);
            hdraw (clrop, xc - xl, xc - xp - 1, yc + yp - 1);
            if ( yp > 1 )
                {
                hdraw (clrop, xc + xp + 1, xc + xl, yc - yp + 1);
                hdraw (clrop, xc - xl, xc - xp - 1, yc - yp + 1);
                }
            }
        dx = xl - xp;
        if ( dx <= 0 ) dx = 1;
        xl = xp;
        ++yp;
        }
    if ( ! bFill )
        {
#if DEBUG & 4
        printf ("yp = %d: final dash 0 - %d\n", yp, xl);
#endif
        hdraw (clrop, xc - xl, xc + xl, yc + yp - 1);
        if ( yp > 1 )
            hdraw (clrop, xc - xl, xc + xl, yc - yp + 1);
        }
    }

static void plotcir (bool bFill, int clrop)
    {
    int xd = pltpt[0].x - pltpt[1].x;
    int yd = pltpt[0].y - pltpt[1].y;
    int r2 = xd * xd + yd * yd;
#if DEBUG & 2
    printf ("plotcir: (%d, %d) (%d, %d), r2 = %d\n", pltpt[1].x, pltpt[1].y, pltpt[0].x,
        pltpt[0].y, r2);
#endif
    if ( r2 < 4 ) clippoint (clrop, pltpt[1].x >> xshift, pltpt[1].y >> yshift);
    else ellipse (bFill, clrop, pltpt[1].x >> xshift, pltpt[1].y >> yshift,
        pixelx << xshift, pixely << yshift, r2);
    }

static void plotellipse (bool bFill, int clrop)
    {
    int xc = pltpt[2].x >> xshift;
    int yc = pltpt[2].y >> yshift;
    int xa = pltpt[1].x - pltpt[2].x;
    int yb = pltpt[0].y - pltpt[2].y;
    if ( xa < 0 ) xa = - xa;
    if ( yb < 0 ) yb = - yb;
    if (( xa < pixelx ) && ( yb < pixely ))
        {
        clippoint (clrop, xc, xc);
        }
    else if ( yb < pixely )
        {
        xa >>= xshift;
        hdraw (clrop, xc - xa, xc + xa, yc);
        }
    else if ( xa < 2 )
        {
        yb >>= yshift;
        clipline (clrop, xc, yc - yb, xc, yc + yb, 0xFFFFFFFF, SKIP_NONE);
        }
    else
        {
        xa *= xa;
        yb *= yb;
        uint64_t dd = ((uint64_t) xa) * ((uint64_t) yb);
        xa <<= 2 * yshift;
        yb <<= 2 * xshift;
        ellipse (bFill, clrop, xc, yc, yb, xa, dd);
        }
    }

static int octant (int xp, int yp)
    {
#if DEBUG & 2
    printf ("octant (%d, %d)\n", xp, yp);
#endif
    xp <<= xshift;
    yp <<= yshift;
#if DEBUG & 2
    printf ("scaled: xp = %d, yp = %d\n", xp, yp);
#endif
    int iOct;
    if ( yp >= 0 )
        {
        if ( xp >= 0 )
            {
            if ( yp < xp ) iOct = 0;
            else iOct = 1;
            }
        else
            {
            if ( yp > -xp ) iOct = 2;
            else iOct = 3;
            }
        }
    else
        {
        if ( xp < 0 )
            {
            if ( xp < yp ) iOct = 4;
            else iOct = 5;
            }
        else
            {
            if ( xp < -yp ) iOct = 6;
            else iOct = 7;
            }
        }
#if DEBUG & 2
    printf ("octant = %d\n", iOct);
#endif
    return iOct;
    }

static void arc (int clrop)
    {
#if DEBUG & 2
    printf ("arc ((%d, %d), (%d, %d), (%d, %d))\n", pltpt[0].x, pltpt[0].y, pltpt[1].x, pltpt[1].y,
        pltpt[2].x, pltpt[2].y);
#endif
    int xc = pltpt[2].x >> xshift;
    int yc = pltpt[2].y >> yshift;
    int xp = pltpt[1].x - pltpt[2].x;
    int yp = pltpt[2].y - pltpt[1].y;
    int xe = pltpt[0].x - pltpt[2].x;
    int ye = pltpt[2].y - pltpt[0].y;
    int r2 = xp * xp + yp * yp;
    int s1 = iroot (r2 << 8);
    int s2 = iroot ((xe * xe + ye * ye) << 8);
    xe = xe * s1 / s2;
    ye = ye * s1 / s2;
    pltpt[0].x = pltpt[2].x + xe;
    pltpt[0].y = pltpt[2].y - ye;
    int iOct = octant (xp, yp);
    int iEnd = octant (xe, ye);
    if ( iEnd < iOct )
        {
        iEnd += 8;
        }
    else if ( iEnd == iOct )
        {
        switch (iOct)
            {
            case 0:
                if ( ye < yp ) iEnd += 8;
                break;
            case 1:
            case 2:
                if ( xe > xp ) iEnd += 8;
                break;
            case 3:
            case 4:
                if ( ye > yp ) iEnd += 8;
                break;
            case 5:
            case 6:
                if ( xe < xp ) iEnd += 8;
                break;
            case 7:
                if ( ye < yp ) iEnd += 8;
                break;
            }
        }
#if DEBUG & 2
    printf ("Arc from: %d: (%d, %d), to: %d: (%d, %d)\n", iOct, xp, yp, iEnd, xe, ye);
#endif
    bool bDone = false;
    xp >>= xshift;
    yp >>= yshift;
    xe >>= xshift;
    ye >>= yshift;
    int xs = 2 * xshift;
    int ys = 2 * yshift;
#if DEBUG & 2
    printf ("Octant %d\n", iOct);
#endif
    while (! bDone)
        {
        clippoint (clrop, xc + xp, yc - yp);
        switch (iOct & 0x07)
            {
            case 0:
                ++yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --xp;
                if ( (yp << ys) >= (xp << xs) )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp > ye )) bDone = true;
                break;
            case 1:
                --xp;
                ++yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --yp;
                if ( xp <= 0 )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp < xe )) bDone = true;
                break;
            case 2:
                --xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --yp;
                if ( (yp << ys) <= ((-xp) << xs) )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp < xe )) bDone = true;
                break;
            case 3:
                --yp;
                --xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++xp;
                if ( yp <= 0 )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp < ye )) bDone = true;
                break;
            case 4:
                --yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++xp;
                if ( (yp << ys) <= (xp << xs) )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp < ye )) bDone = true;
                break;
            case 5:
                ++xp;
                --yp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++yp;
                if ( xp >= 0 )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp > xe )) bDone = true;
                break;
            case 6:
                ++xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) ++yp;
                if ( (xp << xs) >= ((-yp) << ys) )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( xp > xe )) bDone = true;
                break;
            case 7:
                ++yp;
                ++xp;
                if ( ((xp * xp) << xs) + ((yp * yp) << ys) > r2 ) --xp;
                if ( yp >= 0 )
                    {
                    ++iOct;
#if DEBUG & 2
                    printf ("Octant %d\n", iOct);
#endif
                    }
                if (( iOct >= iEnd ) && ( yp > ye )) bDone = true;
                break;
            }
        }
    }

static void plot (uint8_t code, int16_t xp, int16_t yp)
    {
#if DEBUG & 2
    printf ("plot (0x%02X, %d, %d)\n", code, xp, yp);
#endif
    if ( ( code & 0x04 ) == 0 )
        {
#if DEBUG & 2
        printf ("Relative position\n");
#endif
        newpt (pltpt[0].x + xp, pltpt[0].y - yp);
        }
    else
        {
#if DEBUG & 2
        printf ("Absolute position\n");
#endif
        newpt (xp + origx, (pmode->grow << yshift) - 1 - ( yp + origy ));
        }
#if DEBUG & 2
    printf ("origin: (%d, %d)\n", origx, origy);
    printf ("pltpt: (%d, %d) (%d, %d) (%d, %d)\n", pltpt[0].x, pltpt[0].y,
        pltpt[1].x, pltpt[1].y, pltpt[2].x, pltpt[2].y);
    printf ("pixelx = %d, xshift = %d, pixely = %d, yshift = %d\n", pixelx, xshift, pixely, yshift);
#endif
    int clrop;
    switch (code & 0x03)
        {
        case 0:
            return;
        case 1:
            clrop = forgnd;
            break;
        case 2:
            clrop = 0x400;
            break;
        case 3:
            clrop = bakgnd;
            break;
        }
    if ( code < 0x40 )
        {
        static const uint32_t style[] = { 0xFFFFFFFF, 0x33333333, 0x1F1F1F1F, 0x1C7F1C7F };
        clipline (clrop, pltpt[1].x >> xshift, pltpt[1].y >> yshift,
            pltpt[0].x >> xshift, pltpt[0].y >> yshift,
            style[(code >> 4) & 0x03], ( code & 0x08 ) ? SKIP_LAST : SKIP_NONE);
        return;
        }
    switch ( code & 0xF8 )
        {
        case 0x40:
            clippoint (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift);
            break;
        case 0x48:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_LEFT | FT_RIGHT, clrmsk (bakgnd));
            break;
        case 0x50:
            triangle (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                pltpt[1].x >> xshift, pltpt[1].y >> yshift,
                pltpt[2].x >> xshift, pltpt[2].y >> yshift);
            break;
        case 0x55:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_RIGHT | FT_EQUAL, clrmsk (bakgnd));
            break;
        case 0x60:
            rectangle (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                pltpt[1].x >> xshift, pltpt[1].y >> yshift);
            break;
        case 0x68:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_LEFT | FT_RIGHT | FT_EQUAL, clrmsk (forgnd));
            break;
        case 0x70:
            trapizoid (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                pltpt[1].x >> xshift, pltpt[1].y >> yshift,
                pltpt[2].x >> xshift, pltpt[2].y >> yshift);
            break;
        case 0x78:
            linefill (clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift,
                FT_RIGHT, clrmsk (forgnd));
            break;
        case 0x80:
            flood (true, clrmsk (bakgnd), clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift);
            break;
        case 0x88:
            flood (false, clrmsk (forgnd), clrop, pltpt[0].x >> xshift, pltpt[0].y >> yshift);
            break;
        case 0x90:
        case 0x98:
            plotcir (code >= 0x98, clrop);
            break;
        case 0xA0:
            arc (clrop);
            break;
        case 0xC0:
        case 0xC8:
            plotellipse (code >= 0xC8, clrop);
            break;
        default:
            error (255, "Sorry, PLOT function not implemented");
            break;
        }
    }

static void plotchr (int clrop, int xp, int yp, int chr)
    {
    hidecsr ();
    uint32_t fhgt = pmode->thgt;
    bool bDbl = false;
    if ( fhgt > 10 )
        {
        fhgt /= 2;
        bDbl = true;
        }
    fhgt = 8;
    const uint8_t *pch = &fontmap[chr >> 5][8 * (chr & 0x1F)];
    for (int i = 0; i < fhgt; ++i)
        {
        int xx = xp;
        int pix = *pch;
        while ( pix )
            {
            if ( pix & 0x80 ) clippoint (clrop, xx, yp);
            pix <<= 1;
            ++xx;
            }
        ++yp;
        if ( bDbl )
            {
            xx = xp;
            pix = *pch;
            while ( pix )
                {
                if ( pix & 0x80 ) clippoint (clrop, xx, yp);
                pix <<= 1;
                ++xx;
                }
            ++yp;
            }
        ++pch;
        }
    showcsr ();
    }

static void hrwrap (int *pxp, int *pyp)
    {
    int xp = pltpt[0].x >> xshift;
    int yp = pltpt[0].y >> yshift;
    if ( xp + 7 > gvr )
        {
        xp = gvl;
        yp += pmode->thgt;
        }
    if ( yp + pmode->thgt - 1 > gvb )
        {
        yp = gvt;
        }
    newpix (xp, yp);
    if ( pxp != NULL ) *pxp = xp;
    if ( pyp != NULL ) *pyp = yp;
    }

static void hrback (void)
    {
    int xp = pltpt[0].x >> xshift;
    int yp = pltpt[0].y >> yshift;
    if ( xp < gvl )
        {
        int wth = ( gvr - gvl ) / 8;
        xp = gvl + 8 * ( wth - 1 );
        yp -= pmode->thgt;
        }
    if ( yp < gvt )
        {
        int hgt = ( gvb - gvt ) / pmode->thgt;
        yp = gvt + ( hgt - 1 ) * pmode->thgt;
        }
    newpix (xp, yp);
    }

static void showchr (int chr)
    {
    hidecsr ();
    if ( vflags & HRGFLG )
        {
        int xp;
        int yp;
        hrwrap (&xp, &yp);
        plotchr (forgnd, xp, yp, chr);
        newpix (xp + 8, yp);
        if ( (cmcflg & 1) == 0 ) hrwrap (NULL, NULL);
        }
    else
        {
        if (xcsr > tvr) wrap ();
        dispchr (chr);
        if ((++xcsr > tvr) && ((cmcflg & 1) == 0)) wrap ();
        }
    if ( bPrint ) putchar (chr);
    showcsr ();
    }

static void defchr (uint8_t chr, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
    uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
    {
    int iBlk = chr >> 5;
    uint8_t *pblk = fontmap[iBlk];
    if ( pblk < (uint8_t *) userRAM )
        {
        if ( usrchr + 0x100 > (char *) vpage )
            error (255, "Insufficient room below PAGE");
        pblk = (uint8_t *) usrchr;
        usrchr += 0x100;
        memcpy (pblk, fontmap[iBlk], 0x100);
        fontmap[iBlk] = pblk;
        }
    pblk += 8 * (chr & 0x1F);
    *pblk = b0;
    *(++pblk) = b1;
    *(++pblk) = b2;
    *(++pblk) = b3;
    *(++pblk) = b4;
    *(++pblk) = b5;
    *(++pblk) = b6;
    *(++pblk) = b7;
    }

/* Process character sequences:
   code (bits 8-15)     - Control byte
   code (bits 0-7)      - Last parameter byte
   data1 (bits 24-31)   - 2nd from last parameter byte
   data1 (bits 16-23)   - 3rd from last parameter byte
   data1 (bits 8-15)    - 4th from last parameter byte
   data1 (bits 0-7)     - 5th from last parameter byte
   data2 (bits 24-31)   - 6th from last parameter byte
   data2 (bits 16-23)   - 7th from last parameter byte
   data2 (bits 8-15)    - 8th from last parameter byte
   data2 (bits 0-7)     - 9th from last parameter byte

   The control byte is always in the higher byte of code.
   Successive parameter bytes then effectively push earlier
   parameters further down the above list.
*/

// 0x00 - NULL
static void vdu_0 (int code, int data1, int data2)
    {
    return;
    }
            
// 0x01 - Next character to printer only
static void vdu_1 (int code, int data1, int data2)
    {
    putchar (code & 0xFF);
    return;
    }
            
// 0x02 - PRINTER ON
static void vdu_2 (int code, int data1, int data2)
    {
#ifdef HAVE_PRINTER
    bPrint = true;
#endif
    return;
    }

// 0x03 - PRINTER OFF
static void vdu_3 (int code, int data1, int data2)
    {
#ifdef HAVE_PRINTER
    bPrint = false;
#endif
    return;
    }

// 0x04 - LO-RES TEXT
static void vdu_4 (int code, int data1, int data2)
    {
    vflags &= ~HRGFLG;
    return;
    }

// 0x05 - HI-RES TEXT
static void vdu_5 (int code, int data1, int data2)
    {
    vflags |= HRGFLG;
    return;
    }

// 0x06 - ENABLE VDU DRIVERS
static void vdu_6 (int code, int data1, int data2)
    {
    vflags &= ~VDUDIS;
    return;
    }

// 0x07 - BELL
static void vdu_7 (int code, int data1, int data2)
    {
    bell ();
    if ( bPrint ) putchar (0x07);
    return;
    }

// 0x08 - LEFT
static void vdu_8 (int code, int data1, int data2)
    {
    if ( vflags & HRGFLG )
        {
        pltpt[0].x -= 16;
        hrback ();
#if VT100_PRT
        if ( bPrint ) putchar (0x08);
#endif
        }
    else
        {
        if (xcsr == tvl)
            {
            xcsr = tvr;
            if (ycsr == tvt)
                {
                scrldn ();
#if VT100_PRT
                if ( bPrint ) printf ("\033M");
#endif
                }
            else
                {
                ycsr--;
#if VT100_PRT
                if ( bPrint ) printf ("\033[%i;%iH", ycsr + 1, xcsr + 1);
#endif
                }
            }
        else
            {
            xcsr--;
#if VT100_PRT
            if ( bPrint ) putchar (0x08);
#endif
            }
        }
#if ! VT100_PRT
    if ( bPrint ) putchar (0x08);
#endif
    return;
    }

// 0x09 - RIGHT
static void vdu_9 (int code, int data1, int data2)
    {
    if ( vflags & HRGFLG )
        {
        pltpt[0].x += 16;
        hrwrap (NULL, NULL);
        }
    else
        {
        if (xcsr > tvr)
            {
            wrap ();
            }
        else
            {
            xcsr++;
            }
        }
#if VT100_PRT
    if ( bPrint ) printf ("\033[C");
#else
    if ( bPrint ) putchar (0x09);
#endif
    return;
    }

// 0x0A - LINE FEED
static void vdu_10 (int code, int data1, int data2)
    {
    if ( vflags & HRGFLG )
        {
        pltpt[0].y += 2 * pmode->thgt;
        hrwrap (NULL, NULL);
        if ( bPrint ) putchar (0x0A);
        }
    else
        {
        newline (&xcsr, &ycsr);
        }
    return;
    }

// 0x0B - UP
static void vdu_11 (int code, int data1, int data2)
    {
    if ( vflags & HRGFLG )
        {
        pltpt[0].y -= 2 * pmode->thgt;
        hrback ();
        }
    else
        {
        if ( ycsr == tvt ) scrldn ();
        else --ycsr;
        }
#if VT100_PRT
    if ( bPrint ) printf ("\033M");
#endif
    return;
    }

// 0x0C - CLEAR SCREEN
static void vdu_12 (int code, int data1, int data2)
    {
    cls ();
#if VT100_PRT
    if ( bPrint ) printf ("\033[H\033[J");
#else
    if ( bPrint ) putchar (0x0C);
#endif
    return;
    }

// 0x0D - RETURN
static void vdu_13 (int code, int data1, int data2)
    {
    if ( vflags & HRGFLG )
        {
        newpix (gvl, pltpt[0].y >> yshift);
        }
    else
        {
        xcsr = tvl;
        }
    if ( bPrint ) putchar (0x0D);
    return;
    }

// 0x0E - PAGING ON
static void vdu_14 (int code, int data1, int data2)
    {
    scroln = 0x80 + tvb - ycsr + 1;
    return;
    }

// 0x0F - PAGING OFF
static void vdu_15 (int code, int data1, int data2)
    {
    scroln = 0;
    return;
    }

// 0x10 - CLEAR GRAPHICS SCREEN
static void vdu_16 (int code, int data1, int data2)
    {
    if ( pmode->ncbt != 3 )
        {
        clrgraph ();
#if VT100_PRT
        if ( bPrint ) printf ("\033[H\033[J");
#endif
        return;
        }
    }

// 0x11 - COLOUR n
static void vdu_17 (int code, int data1, int data2)
    {
    if ( code & 0x80 )
        {
        txtbak = clrmsk (code);
        }
    else
        {
        txtfor = clrmsk (code);
#if DEBUG & 2
        printf ("Foreground colour %d\n", txtfor);
#endif
        }
#if VT100_PRT
    if ( bPrint )
        {
        int vdu;
        vdu = 30 + (code & 7);
        if (code & 8)
            vdu += 60;
        if (code & 0x80)
            vdu += 10;
        printf ("\033[%im", vdu);
        }
#endif
    return;
    }

// 0x12 - GCOL m, n
static void vdu_18 (int code, int data1, int data2)
    {
    if ( code & 0x80 )
        {
        bakgnd = clrmsk (code) | (( data1 >> 16 ) & 0x0700);
        }
    else
        {
        forgnd = clrmsk (code) | (( data1 >> 16 ) & 0x0700);
        }
#if VT100_PRT
    if ( bPrint )
        {
        int vdu;
        vdu = 30 + (data1 & 7);
        if (code & 8)
            vdu += 60;
        if (code & 0x80)
            vdu += 10;
        printf ("\033[%im", vdu);
        }
#endif
    return;
    }

// 0x13 - SET CURPAL
static void vdu_19 (int code, int data1, int data2)
    {
    int pal = data1 & 0x0F;
    int phy = ( data1 >> 8 ) & 0xFF;
    int r = ( data1 >> 16 ) & 0xFF;
    int g = ( data1 >> 24 ) & 0xFF;;
    int b = code & 0xFF;
#if DEBUG & 2
    printf ("pal = %d, phy = %d, r = %d, g = %d, b = %d\n", pal, phy, r, g, b);
#endif
    if (pmode->ncbt == 3)
        {
        clrset (2 * pal, phy, r, g, b);
        clrset (2 * pal + 1, phy, r, g, b);
        }
    else
        {
        clrset (pal, phy, r, g, b);
        }
    }

// 0x14 - RESET COLOURS
static void vdu_20 (int code, int data1, int data2)
    {
    txtbak = 0;
    bakgnd = 0;
    txtfor = (1 << pmode->ncbt) - 1;
    forgnd = txtfor;
    clrreset ();
#if VT100_PRT
    if ( bPrint ) printf ("\033[37m\033[40m");
#endif
    return;
    }

// 0x15 - DISABLE VDU DRIVERS
static void vdu_21 (int code, int data1, int data2)
    {
    vflags |= VDUDIS;
    return;
    }

// 0x16 - MODE CHANGE
static void vdu_22 (int code, int data1, int data2)
    {
    modechg (code & 0x7F);
    return;
    }

// 0x17 - DEFINE CHARACTER ETC.
static void vdu_23 (int code, int data1, int data2)
    {
#if DEBUG & 2
    printf ("VDU 0x17: code = 0x%04X, data1 = 0x%08X, data2 = 0x%08X\n",
        code, data1, data2);
#endif
    int vdu = data2 & 0xFF;
    if ( vdu <= 1 )
        {
        csrdef (data2);
        }
    else if ( vdu == 16 )
        {
        uint8_t a = (data2 >> 8) & 0xFF;
        uint8_t b = (data2 >> 16) & 0xFF;
        cmcflg = (cmcflg & b) ^ a;
        }
    else if ( vdu >= 32 )
        {
        defchr (vdu, (data2 >> 8) & 0xFF, (data2 >> 16) & 0xFF,
            (data2 >> 24) & 0xFF, data1 & 0xFF, (data1 >> 8) & 0xFF,
            (data1 >> 16) & 0xFF, (data1 >> 24) & 0xFF, code & 0xFF);
        }
    return;
    }

// 0x18 - DEFINE GRAPHICS VIEWPORT
static void vdu_24 (int code, int data1, int data2)
    {
    gwind ((data2 >> 8) & 0xFFFF,
        ((data2 >> 24) & 0xFF) | ((data1 & 0xFF) << 8),
        (data1 >> 8) & 0xFFFF,
        ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8));
    return;
    }

// 0x19 - PLOT
static void vdu_25 (int code, int data1, int data2)
    {
    if ( pmode->ncbt != 3 )
        plot (data1 & 0xFF, (data1 >> 8) & 0xFFFF,
            ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8));
    return;
    }

// 0x1A - RESET VIEWPORTS
static void vdu_26 (int code, int data1, int data2)
    {
    rstview ();
    return;
    }

// 0x1B - SEND NEXT TO OUTC
static void vdu_27 (int code, int data1, int data2)
    {
    showchr (code & 0xFF);
    return;
    }

// 0x1C - SET TEXT VIEWPORT
static void vdu_28 (int code, int data1, int data2)
    {
    twind ((data1 >> 8) & 0xFF, (data1 >> 16) & 0xFF,
        (data1 >> 24) & 0xFF, code & 0xFF);
    return;
    }

// 0x1D - SET GRAPHICS ORIGIN
static void vdu_29 (int code, int data1, int data2)
    {
    origx = (data1 >> 8) & 0xFFFF;
    origy = ((data1 >> 24) & 0xFF) | ((code & 0xFF) << 8);
    return;
    }

// 0x1E - CURSOR HOME
static void vdu_30 (int code, int data1, int data2)
    {
#if VT100_PRT
    if ( bPrint ) printf ("\033[H");
#endif
    home ();
    return;
    }

// 0x1F - TAB(X,Y)
static void vdu_31 (int code, int data1, int data2)
    {
    tabxy (data1 >> 24, code & 0xFF);
#if VT100_PRT
    if ( bPrint ) printf ("\033[%i;%iH", ycsr + 1, xcsr + 1);
#endif
    return;
    }

static void vdu_127 (void)
    {
    xeqvdu (0x0800, 0, 0);
    if ( vflags & HRGFLG )
        {
        rectangle (clrmsk (bakgnd), pltpt[0].x >> xshift, pltpt[0].y >> yshift,
            pltpt[0].x >> xshift + 7, pltpt[0].y >> yshift + pmode->thgt - 1);
        }
    else
        {
        xeqvdu (0x2000, 0, 0);
        xeqvdu (0x0800, 0, 0);
        }
    }

typedef void (*VDU_FUNC)(int code, int data1, int data2);

static const VDU_FUNC vdu_func[] =
    {
    vdu_0,  // 0x00 - NULL
    vdu_1,  // 0x01 - Next character to printer only
    vdu_2,  // 0x02 - PRINTER ON
    vdu_3,  // 0x03 - PRINTER OFF
    vdu_4,  // 0x04 - LO-RES TEXT
    vdu_5,  // 0x05 - HI-RES TEXT
    vdu_6,  // 0x06 - ENABLE VDU DRIVERS
    vdu_7,  // 0x07 - BELL
    vdu_8,  // 0x08 - LEFT
    vdu_9,  // 0x09 - RIGHT
    vdu_10, // 0x0A - LINE FEED
    vdu_11, // 0x0B - UP
    vdu_12, // 0x0C - CLEAR SCREEN
    vdu_13, // 0x0D - RETURN
    vdu_14, // 0x0E - PAGING ON
    vdu_15, // 0x0F - PAGING OFF
    vdu_16, // 0x10 - CLEAR GRAPHICS SCREEN
    vdu_17, // 0x11 - COLOUR n
    vdu_18, // 0x12 - GCOL m, n
    vdu_19, // 0x13 - SET CURPAL
    vdu_20, // 0x14 - RESET COLOURS
    vdu_21, // 0x15 - DISABLE VDU DRIVERS
    vdu_22, // 0x16 - MODE CHANGE
    vdu_23, // 0x17 - DEFINE CHARACTER ETC.
    vdu_24, // 0x18 - DEFINE GRAPHICS VIEWPORT
    vdu_25, // 0x19 - PLOT
    vdu_26, // 0x1A - RESET VIEWPORTS
    vdu_27, // 0x1B - SEND NEXT TO OUTC
    vdu_28, // 0x1C - SET TEXT VIEWPORT
    vdu_29, // 0x1D - SET GRAPHICS ORIGIN
    vdu_30, // 0x1E - CURSOR HOME
    vdu_31  // 0x1F - TAB(X,Y)
    };
    

void xeqvdu (int code, int data1, int data2)
    {
    int vdu = code >> 8;

#if DEBUG & 1
    if ((vdu > 32) && (vdu < 127)) printf ("%c", vdu);
    else if ((vdu == 10) || (vdu == 13)) printf ("%c", vdu);
    else if (vdu == 32) printf ("_");
    else printf (" 0x%02X ", vdu);
    fflush (stdout);
#endif

#if REF_MODE > 0
    if ( reflag == 0 )
        {
#if DEBUG & 2
        printf ("reflag = 0\n");
#endif
        refresh_on ();
        }
#endif
    if ( reflag == 1 )
        {
#if REF_MODE == 1
        const char *psErr = checkbuf ();
        if ( psErr )
            {
            refresh_on ();
            error (63, psErr);
            }
#elif REF_MODE == 2
        vduqueue (code, data1, data2);
        return;
#elif REF_MODE == 3
        if ( rfm == rfmQueue )
            {
            vduqueue (code, data1, data2);
            return;
            }
        else if ( rfm == rfmBuffer )
            {
            const char *psErr = checkbuf ();
            if ( psErr )
                {
                refresh_on ();
                error (63, psErr);
                }
            }
#endif
        }

    if ((vflags & VDUDIS) && (vdu != 6))
        return;

    hidecsr ();
    if ( vdu < ' ' )
        {
        vdu_func[vdu](code, data1, data2);
        }
    else if ( vdu == 0x7F )
        {
        vdu_127 ();
        }
    else
        {
        showchr (vdu);
        }
    showcsr ();
    textx = 8 * xcsr;
    texty = pmode->thgt * ycsr;
    if ( bPrint ) fflush (stdout);
    }

bool txtmode (int code, int *pdata1, int *pdata2)
    {
    const MODE *pm = modeinfo(code & 0x7F);
    if (pm == NULL) return false;
    *pdata2 = (pm->gcol << 8) | (pm->grow << 24);
    *pdata1 = (pm->grow >> 8) | 0x0800 | (pm->thgt << 16) | (0x01000000 << pm->ncbt);
    return true;
    }

#if REF_MODE & 2
void vduflush (void)
    {
    char resave = reflag;
    reflag = 2;
    int *queptr = vduqbot;
#if DEBUG & 4
    printf ("vduflush: libtop = %p, queptr = %p, vduque = %p\n", libtop, queptr, vduque);
#endif
    while ( queptr < vduque )
        {
        int code = *(queptr);
        int data1 = *(++queptr);
        int data2 = *(++queptr);
        ++queptr;
        xeqvdu (code, data1, data2);
        }
#if DEBUG & 4
    printf ("vduflush: completed\n");
#endif
    vduque = vduqbot;
    reflag = resave;
    }

void vduqueue (int code, int data1, int data2)
    {
    if ( vduque >= vduqtop )
        {
#if DEBUG & 4
        printf ("Flush due to queue full\n");
#endif
        vduflush ();
        }
    *vduque = code;
    *(++vduque) = data1;
    *(++vduque) = data2;
    ++vduque;
    }

void vduqinit (void)
    {
    void *sp = &sp;
    if ( libase - himem >= 12 * nRefQue + 4)
        {
        vduqbot = (int *)(((int)himem + 3) & 0xFFFFFFFC);
        vduqtop = vduqbot + nRefQue;
        }
    else
        {
        if ( libase == 0 ) vduqbot = himem;
        else vduqbot = libtop;
        vduqbot = (int *)(((int)vduqbot + 3) & 0xFFFFFFFC);
        vduqtop = vduqbot + nRefQue;
        if ( (void *)vduqtop + 0x280 < sp )
            {
            libtop = vduqtop;
            oshwm (libtop, 0);
            }
        else
            {
            error (255, "No room for refresh buffer");
            }
        }
    }

void vduqterm (void)
    {
    vduflush ();
    if ( libtop == vduqtop )
        {
        libtop = vduqbot;
        oshwm (libtop, 0);
        }
    }
#endif

#if REF_MODE == 2
void refresh_now (void)
    {
#if DEBUG & 4
    printf ("refresh now\n");
#endif
    vduflush ();
    }

void refresh_on (void)
    {
#if DEBUG & 2
    printf ("refresh on\n");
#endif
    vduqinit ();
    reflag = 2;
    }

void refresh_off (void)
    {
#if DEBUG & 2
    printf ("refresh off\n");
#endif
    vduqterm ();
    reflag = 1;
    }
#endif

void refresh (const char *p)
    {
#if REF_MODE > 0
    while ( *p == ' ' ) ++p;
    if ( *p == 0x0D )
        {
        refresh_now ();
        }
    else if ( ! strncasecmp (p, "on", 2) )
        {
        refresh_on ();
        }
    else if ( ! strncasecmp (p, "off", 3) )
        {
        refresh_off ();
        }
#endif
#if REF_MODE == 3
    else if ( ! strncasecmp (p, "none", 4) )
        {
#if DEBUG & 2
        printf ("refresh none\n");
#endif
        refresh_on ();
        rfm = rfmNone;
        }
    else if ( ! strncasecmp (p, "buffer", 6) )
        {
#if DEBUG & 2
        printf ("refresh buffer\n");
#endif
        refresh_on ();
        rfm = rfmBuffer;
        }
    else if ( ! strncasecmp (p, "queue", 6) )
        {
#if DEBUG & 2
        printf ("refresh queue\n");
#endif
        refresh_on ();
        rfm = rfmQueue;
        while ( *p == ' ' ) ++p;
        if ((*p >= '0') && (*p <= '9'))
            {
            nRefQue = 0;
            while ((*p >= '0') && (*p <= '9'))
                {
                nRefQue = 10 * nRefQue + *p - '0';
                ++p;
                }
            }
        else
            {
            nRefQue = REFQ_DEF;
            }
        }
#endif
    }
