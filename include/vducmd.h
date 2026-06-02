/* vducmd.h - Hardware interface for implementing VDU commands */

#ifndef H_VDUCMD
#define H_VDUCMD

#include <stdint.h>
#include <stdbool.h>

//  REF_MODE =      0   Not implemented
//                  1   Using double buffering
//                  2   Using VDU queue
//                  3   User configurable
#ifndef REF_MODE
#define REF_MODE    3
#endif
#ifndef REFQ_DEF
#define REFQ_DEF    1024        // Default length for VDU queue
#endif

#define CSR_OFF 0x80                    // Cursor turned off
#define CSR_INV 0x40                    // Cursor invalid (off-screen)
#define CSR_CNT 0x3F                    // Cursor hide count bits

#ifdef PICO_GRAPH
#define xeqvdu graphvdu
#define getcsr getcsrgr
#define vpoint vpointgr
#define vtint vtintgr
#define vgetc vgetcgr
#define widths widthsgr
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

// Variables defined in vducmd.c
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
#if REF_MODE == 3
typedef enum { rfmNone, rfmBuffer, rfmQueue } RFM;
extern RFM rfm;
#endif
#if defined(PICO_GUI) || defined(PICO_GRAPH)
extern int xccsr;                       // Copy cursor horizontal position (-1 to disable)
extern int yccsr;                       // Copy cursor vertical position (-1 to disable)
#endif

// VDU variables declared in bbcdata_*.s:
extern void *vpage;                     // Location of PAGE (bottom of program)
extern int origx;                       // Graphics x-origin (BASIC units)
extern int origy;                       // Graphics y-origin (BASIC units)
extern int lastx;                       // Graphics cursor x-position (pixels)
extern int lasty;                       // Graphics cursor y-position (pixels)
extern int prevx;                       // Previous Graphics cursor x-position (pixels)
extern int prevy;                       // Previous Graphics cursor y-position (pixels)
extern unsigned char cursa;             // Start (top) line of cursor
extern unsigned char cursb;             // Finish (bottom) line of cursor
extern int textwl;                      // Text window left (pixels)
extern int textwr;                      // Text window right (pixels)
extern int textwt;                      // Text window top (pixels)
extern int textwb;                      // Text window bottom (pixels)
extern int pixelx;                      // Width of a graphics pixel
extern int pixely;                      // Height of a graphics pixel
extern int textx;                       // Text caret x-position (pixels)
extern int texty;                       // Text caret y-position (pixels)
extern short int forgnd;                // Graphics foreground colour/action
extern short int bakgnd;                // Graphics background colour/action
extern unsigned char txtfor;            // Text foreground colour
extern unsigned char txtbak;            // Text background colour
extern int bPaletted;                   // @ispal%
extern int sizex;                       // Total width of client area
extern int sizey;                       // Total height of client area
extern int charx;                       // Average character width
extern int chary;                       // Average character height

bool csrpos (int *px, int *py);         // Cursor position in pixels units, false if outside viewport
void home (void);                       // Home cursor
void modechg (int mode);                // Change display mode
void getcsr(int *px, int *py);          // Get position of text cursor (in characters) relative to viewport
void tabxy (int x, int y);              // Set position of text cursor (in characters) relative to viewport
int vpoint (int xp, int yp);            // Get colour index at position in graphics units
int vtint (int xp, int yp);             // Get RGB value at position in graphics units
int vgetc (int x, int y);               // Get character at position (in characters) relative to viewport
int widths (unsigned char *s, int l);   // Length in graphics units of string of length l
void xeqvdu (int code, int data1, int data2);       // Execute VDU command with parameters
bool txtmode (int code, int *pdata1, int *pdata2);  // Get definition of a mode
#if REF_MODE & 2
void vduflush (void);
void vduqueue (int code, int data1, int data2);
void vduqinit (void);
void vduqterm (void);
#endif
#if REF_MODE & 1
const char *checkbuf (void);
#endif
#if REF_MODE
void refresh_now (void);
void refresh_on (void);
void refresh_off (void);
void refresh (const char *p);
#endif
void prtscrn (void);
#ifdef PICO_GUI
void copyedit (bool bEnable);
void copymove (int key);
int copyread (void);
#endif

// Routines that have to be provided by hardware specific drivers

const MODE *setmode (int mode);
void dispenable (void);
void hidecsr (void);
void showcsr (void);
void enablecsr (bool bEnable);
void scrldn (void);
void scrlup (void);
void cls ();
void disp_ttx (char chr);
void disp_glyph (uint8_t *pch);
void point (int clrop, uint32_t xp, uint32_t yp);
void hline (int clrop, int xp1, int xp2, int yp);
void clrgraph (void);
uint8_t getpix (int xp, int yp);
int get_ttx (int x, int y);
void get_glyph (int x, int y, uint8_t *prow);
void gsize (uint32_t *pwth, uint32_t *phgt);
int clrrgb (int clr);
void clrreset (void);
void clrset (int pal, int phy, int r, int g, int b);
const MODE *modeinfo (int mode);

#endif
