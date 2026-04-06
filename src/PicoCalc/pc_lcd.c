/* pc_lcd.c - Output to the PicoCalc LCD */

#include <stdio.h>
#include <string.h>
#include <hardware/sync.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <pico/time.h>
#include "fbufvdu.h"
#include "lfswrap.h"
#include "bbccon.h"
#include "picocli.h"
#include "periodic.h"

#include "font_tt.h"

#define RGB     16

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
extern short int forgnd;                // Graphics foreground colour/action
extern short int bakgnd;                // Graphics background colour/action
extern unsigned char txtfor;            // Text foreground colour
extern unsigned char txtbak;            // Text background colour

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

#if RGB == 18
#define colour_t uint32_t
#define PIXEL_FROM_RGB8(r, g, b)    (((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b))
#else
#define colour_t uint16_t
#define PIXEL_FROM_RGB8(r, g, b)    (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#endif

static colour_t curpal[16];             // Current palette

static const colour_t defpal[16] =
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

static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 320, 256,  40, 32,  32,  0,  8,  40, 0,  8},   // Mode  0 - 10KB
    { 2, 320, 256,  40, 32,  32,  0,  4,  80, 0,  8},   // Mode  1 - 20KB
    { 4, 160, 256,  20, 32,  32,  0,  4,  80, 0,  8},   // Mode  2 - 20KB
    { 1, 320, 225,  40, 25,  47,  0,  8,  40, 0,  9},   // Mode  3 - 10KB
    { 1, 320, 256,  40, 32,  32,  0,  8,  40, 0,  8},   // Mode  4 - 10KB
    { 2, 160, 256,  20, 32,  32,  0,  8,  40, 0,  8},   // Mode  5 - 10KB
    { 1, 320, 225,  40, 25,  47,  0,  8,  40, 0,  9},   // Mode  6 - 10KB
    { 3, 320, 225,  40, 25,  47,  0,  8, 160, 0,TTH},   // Mode  7 - ~1KB - Teletext
    { 1, 320, 320,  40, 20,   0,  0,  8,  40, 0, 16},   // Mode  8 - 19KB
    { 2, 320, 320,  40, 20,   0,  0,  4,  80, 0, 16},   // Mode  9 - 37.5KB
    { 4, 160, 320,  20, 20,   0,  0,  4,  80, 0, 16},   // Mode 10 - 37.5KB
    { 1, 320, 288,  40, 16,  15,  0,  8,  40, 0, 18},   // Mode 11 - 19KB
    { 1, 320, 320,  40, 40,   0,  0,  8,  40, 0,  8},   // Mode 12 - 19KB
    { 2, 320, 320,  40, 40,   0,  0,  4,  80, 0,  8},   // Mode 13 - 37.5KB
    { 4, 160, 320,  20, 40,   0,  0,  4,  80, 0,  8},   // Mode 14 - 37.5KB
    { 1, 320, 288,  40, 32,  15,  0,  8,  40, 0,  9},   // Mode 15 - 19KB
    };

static CLIFUNC excli = NULL;
static MODE curmode;
static uint8_t *framebuf = NULL;
static bool bInverse = false;
static int nrow = 480;
static int scrltop = 0;
static int xscl = 1;
static int yscl = 1;

#include "pico/binary_info.h"
bi_decl (bi_1pin_with_name (PICO_LCD_DC_PIN,    "LCD command / data"));
bi_decl (bi_1pin_with_name (PICO_LCD_CS_PIN,    "LCD chip select"));
bi_decl (bi_1pin_with_name (PICO_LCD_CLK_PIN,   "LCD clock"));
bi_decl (bi_1pin_with_name (PICO_LCD_TX_PIN,    "LCD data send"));
bi_decl (bi_1pin_with_name (PICO_LCD_RX_PIN,    "LCD data receive"));
bi_decl (bi_1pin_with_name (PICO_LCD_RST_PIN,   "LCD reset"));

#ifndef PICO_LCD_RX_PIN
#define PICO_LCD_RX_PIN 16
#endif
#ifndef PICO_LCD_CS_PIN
#define PICO_LCD_CS_PIN 17
#endif
#ifndef PICO_LCD_CLK_PIN
#define PICO_LCD_CLK_PIN 18
#endif
#ifndef PICO_LCD_TX_PIN
#define PICO_LCD_TX_PIN 19
#endif
#ifndef PICO_LCD_DC_PIN
#define PICO_LCD_DC_PIN 14
#endif
#ifndef PICO_LCD_RST_PIN
#define PICO_LCD_RST_PIN 15
#endif

void LCD_SPI_Init (void)
    {
    // init GPIO
    gpio_init (PICO_LCD_CS_PIN);
    gpio_init (PICO_LCD_DC_PIN);
    gpio_init (PICO_LCD_RST_PIN);

    gpio_set_dir (PICO_LCD_CS_PIN, GPIO_OUT);
    gpio_set_dir (PICO_LCD_DC_PIN, GPIO_OUT);
    gpio_set_dir (PICO_LCD_RST_PIN, GPIO_OUT);

    gpio_put (PICO_LCD_CS_PIN, 1);
    gpio_put (PICO_LCD_RST_PIN, 1);

    // init SPI
    gpio_set_function (PICO_LCD_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function (PICO_LCD_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function (PICO_LCD_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled (PICO_LCD_RX_PIN, true);
    int speed = spi_init (SPI_INSTANCE(PICO_LCD_SPI), PICO_LCD_SPEED);
    printf ("LCD speed = %d\n", speed);
    }

void LCD_WriteReg (unsigned char data)
    {
    gpio_put (PICO_LCD_DC_PIN, 0);
    gpio_put (PICO_LCD_CS_PIN, 0);

    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), &data, 1);

    gpio_put (PICO_LCD_CS_PIN, 1);
    }

void LCD_WriteData8 (uint8_t data)
    {
    gpio_put (PICO_LCD_DC_PIN, 1);
    gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), &data, 1);
    gpio_put (PICO_LCD_CS_PIN, 1);
    }

void LCD_WriteData16 (uint32_t data)
    {
    uint8_t data_array[2];
    data_array[0] = (data >> 8) & 0xFF;
    data_array[1] = data & 0xFF;


    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data_array, 2);
    gpio_put (PICO_LCD_CS_PIN, 1);
    }

void LCD_WriteData24 (uint32_t data)
    {
    uint8_t data_array[3];
    data_array[0] = (data >> 16) & 0xFF;
    data_array[1] = (data >> 8) & 0xFF;
    data_array[2] = data & 0xFF;


    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data_array, 3);
    gpio_put (PICO_LCD_CS_PIN, 1);
    }

void LCD_Data_Init (void)
    {
    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    gpio_put (PICO_LCD_CS_PIN, 0);
    }

void LCD_Data_Term (void)
    {
    gpio_put (PICO_LCD_CS_PIN, 1);
    }

void LCD_WriteColour (colour_t clr, int nRpt)
    {
#if RGB == 18
    uint8_t data[3];
    data[0] = (clr >> 16) & 0xFF;
    data[1] = (clr >> 8) & 0xFF;
    data[2] = clr & 0xFF;
    while (nRpt)
        {
        spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data, 2);
        --nRpt;
        }
#else
    uint8_t data[2];
    data[0] = (clr >> 8) & 0xFF;
    data[1] = clr & 0xFF;
    while (nRpt)
        {
        spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data, 2);
        --nRpt;
        }
#endif
    }

void setup_vdu (void)
    {
    LCD_SPI_Init ();
    
    // Reset LCD
    gpio_put (PICO_LCD_RST_PIN, 1);
    sleep_us (10000);
    gpio_put (PICO_LCD_RST_PIN, 0);
    sleep_us (10000);
    gpio_put (PICO_LCD_RST_PIN, 1);
    sleep_us (200000);

    LCD_WriteReg (0xE0); // Positive Gamma Control
    LCD_WriteData8 (0x00);
    LCD_WriteData8 (0x03);
    LCD_WriteData8 (0x09);
    LCD_WriteData8 (0x08);
    LCD_WriteData8 (0x16);
    LCD_WriteData8 (0x0A);
    LCD_WriteData8 (0x3F);
    LCD_WriteData8 (0x78);
    LCD_WriteData8 (0x4C);
    LCD_WriteData8 (0x09);
    LCD_WriteData8 (0x0A);
    LCD_WriteData8 (0x08);
    LCD_WriteData8 (0x16);
    LCD_WriteData8 (0x1A);
    LCD_WriteData8 (0x0F);

    LCD_WriteReg (0XE1); // Negative Gamma Control
    LCD_WriteData8 (0x00);
    LCD_WriteData8 (0x16);
    LCD_WriteData8 (0x19);
    LCD_WriteData8 (0x03);
    LCD_WriteData8 (0x0F);
    LCD_WriteData8 (0x05);
    LCD_WriteData8 (0x32);
    LCD_WriteData8 (0x45);
    LCD_WriteData8 (0x46);
    LCD_WriteData8 (0x04);
    LCD_WriteData8 (0x0E);
    LCD_WriteData8 (0x0D);
    LCD_WriteData8 (0x35);
    LCD_WriteData8 (0x37);
    LCD_WriteData8 (0x0F);

    LCD_WriteReg (0XC0); // Power Control 1
    LCD_WriteData8 (0x17);
    LCD_WriteData8 (0x15);

    LCD_WriteReg (0xC1); // Power Control 2
    LCD_WriteData8 (0x41);

    LCD_WriteReg (0xC5); // VCOM Control
    LCD_WriteData8 (0x00);
    LCD_WriteData8 (0x12);
    LCD_WriteData8 (0x80);

    LCD_WriteReg (0x36); // Memory Access Control
    LCD_WriteData8 (0x48); // MX, BGR

    LCD_WriteReg (0x3A); // Pixel Interface Format
#if RGB == 18
    LCD_WriteData8 (0x66); // 18 bit colour for SPI
#else
    // LCD_WriteData8 (0x55); // 16 bit colour for SPI
    LCD_WriteData8 (0x65); // 16 bit colour for SPI
#endif
    
    LCD_WriteReg (0xB0); // Interface Mode Control
    LCD_WriteData8 (0x00);

    LCD_WriteReg (0xB1); // Frame Rate Control
    LCD_WriteData8 (0xA0);

    LCD_WriteReg (0x21); // Display Inversion On

    LCD_WriteReg (0xB4); // Display Inversion Control
    LCD_WriteData8 (0x02);

    LCD_WriteReg (0xB6); // Display Function Control
    LCD_WriteData8 (0x02);
    LCD_WriteData8 (0x02);
    LCD_WriteData8 (0x3B);

    LCD_WriteReg (0xB7); // Entry Mode Set
    LCD_WriteData8 (0xC6);
    
    LCD_WriteReg (0xE9); // Set image function
#if RGB == 18
    LCD_WriteData8 (0x01);    // 1 = Enable 24-bit input
#else
    LCD_WriteData8 (0x00);    // 1 = Enable 24-bit input
#endif

    LCD_WriteReg (0xF7); // Adjust Control 3 - Is this required?
    LCD_WriteData8 (0xA9);
    LCD_WriteData8 (0x51);
    LCD_WriteData8 (0x2C);
    LCD_WriteData8 (0x82);    // DSI write DCS command, use loose packet RGB 666

    LCD_WriteReg (0x11); //Exit Sleep
    sleep_ms (120);

    LCD_WriteReg (0x29); //Display on
    sleep_ms (120);

    LCD_WriteReg (0x36); // Memory Access Control
    LCD_WriteData8 (0x48); // MX, BGR

    setup_fbuf (&curmode);
    memset (singlebuf (), 0, BUF_SIZE);
    modechg (9);
    }

void LCD_Scroll (int data)
    {
    LCD_WriteReg (0x37);
    LCD_WriteData16 (data);
    }

void LCD_ScrollArea (int top, int mid, int bot)
    {
    LCD_WriteReg (0x33);
    LCD_WriteData16 (top);
    LCD_WriteData16 (mid);
    LCD_WriteData16 (bot);
    }

void LCD_SetWindow (int Xstart, int Ystart,	int Xend, int Yend)
    {	
	//set the X coordinates
	LCD_WriteReg(0x2A);
	LCD_WriteData16 (Xstart);
	LCD_WriteData16 (Xend - 1);

	//set the Y coordinates
	LCD_WriteReg(0x2B);
	LCD_WriteData16 (Ystart);
	LCD_WriteData16 (Yend - 1);

    LCD_WriteReg(0x2C);                 // Start pixel transfer
    }

void LCD_SetCursor (int Xpoint, int Ypoint)
    {
	LCD_SetWindow (Xpoint, Ypoint, Xpoint + 1, Ypoint + 1);
    }

void LCD_SetColour (uint32_t Colour , int Xpoint, int Ypoint)
    {
    LCD_Data_Init ();
    LCD_WriteColour (Colour, (uint32_t)Xpoint * (uint32_t)Ypoint);
    LCD_Data_Term ();
    }

void LCD_SetPointlColour (int Xpoint, int Ypoint, uint32_t Colour)
    {
    if ((Xpoint <= PICO_LCD_COLUMNS) && (Ypoint <= PICO_LCD_ROWS))
        {
        LCD_SetCursor (Xpoint, Ypoint);
        LCD_SetColour(Colour, 1, 1);
        }
    }

void LCD_SetAreaColour (int Xstart, int Ystart, int Xend, int Yend,	uint32_t Colour)
    {
    if((Xend > Xstart) && (Yend > Ystart))
        {
        LCD_SetWindow (Xstart, Ystart, Xend, Yend);
        LCD_SetColour (Colour, Xend - Xstart, Yend - Ystart);
        }
    }

void LCD_Clear (uint32_t Colour)
    {
    LCD_SetAreaColour (0, 0, PICO_LCD_COLUMNS, PICO_LCD_ROWS, Colour);
    }

void pclcd_out (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2)
    {
    bool bWrap = false;
    if (scrltop + (yp1 << curmode.yshf) >= nrow)
        {
        bWrap = true;
        scrltop -= nrow;
        }
    else if (scrltop + (yp2 << curmode.yshf) > nrow)
        {
        pclcd_out (fbuf, xp1, yp1, xp2, (nrow - scrltop) >> curmode.yshf);
        bWrap = true;
        scrltop -= nrow;
        yp1 = (-scrltop) >> curmode.yshf;
        }
    if (fbuf != framebuf) return;
    switch (curmode.ncbt)
        {
        case 1:
            xp1 >>= 3;
            xp2 = ((xp2 - 1) >> 3) + 1;
            break;
        case 2:
            xp1 >>= 2;
            xp2 = ((xp2 - 1) >> 2) + 1;
            break;
        case 4:
            xp1 >>= 1;
            xp2 = ((xp2 - 1) >> 1) + 1;
            break;
        }
    
    LCD_SetWindow (xp1 * curmode.nppb, curmode.vmgn + scrltop + (yp1 << curmode.yshf),
        xp2 * curmode.nppb, curmode.vmgn + scrltop + ((yp2 + 1) << curmode.yshf) - 1);
    fbuf += curmode.nbpl * yp1 + xp1;
    LCD_Data_Init ();
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
                        LCD_WriteColour (curpal[pix & 0x0F], xscl);
                        LCD_WriteColour (curpal[pix >> 4], xscl);
                        break;
                    case 2:
                        LCD_WriteColour (curpal[pix & 0x03], xscl);
                        LCD_WriteColour (curpal[(pix >> 2) & 0x03], xscl);
                        LCD_WriteColour (curpal[(pix >> 4) & 0x03], xscl);
                        LCD_WriteColour (curpal[pix >> 6], xscl);
                        break;
                    case 1:
                        for (int i = 0; i < 8; ++i)
                            {
                            LCD_WriteColour (curpal[pix & 0x01], xscl);
                            pix >>= 1;
                            }
                        break;
                    }
                ++fp;
                }
            }
        fbuf += curmode.nbpl;
        }
    LCD_Data_Term ();
    if (bWrap) scrltop += nrow;
    }

void pclcd_scroll (uint8_t *fbuf, int lt, int lb, bool bUp)
    {
    if ((lt != 0) || (lb != curmode.trow - 1))
        {
        pclcd_out (fbuf, 0, lt * curmode.thgt, curmode.gcol, (lb + 1) * curmode.thgt);
        }
    else if (bUp)
        {
        scrltop += curmode.thgt << curmode.yshf;
        if ( scrltop >= nrow) scrltop -= nrow;
        LCD_Scroll (curmode.vmgn + scrltop);
        pclcd_out (fbuf, 0, (curmode.trow - 1) * curmode.thgt, curmode.gcol, curmode.grow);
        }
    else
        {
        scrltop -= curmode.thgt << curmode.yshf;
        if ( scrltop < 0) scrltop += nrow;
        LCD_Scroll (curmode.vmgn + scrltop);
        pclcd_out (fbuf, 0, 0, curmode.gcol, curmode.thgt);
        }
    }

void bufswap (uint8_t *fbuf)
    {
    if (fbuf == framebuf) pclcd_out (fbuf, 0, 0, curmode.gcol, curmode.grow);
    }

int clrrgb (int clr)
    {
    clr = curpal[clr];
#if RGB == 18
    return clr;
#else
    return ((clr & 0xF800) >> 8) | ((clr & 0x07E0) << 5) | ((clr & 0x001F) << 19);
#endif
    }

void clrreset (const MODE *pmode)
    {
    if ( pmode->ncbt == 1 )
        {
#if DEBUG & 2
        printf ("clrreset: nclr = 2\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[15];
        }
    else if ( pmode->ncbt == 2 )
        {
#if DEBUG & 2
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[9];
        curpal[2] = defpal[11];
        curpal[3] = defpal[15];
        }
    else if ( pmode->ncbt == 3 )
        {
#if DEBUG & 2
        printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        for (int i = 1; i < 8; ++i)
            {
            curpal[i] = defpal[i+8];
            }
        }
    else
        {
#if DEBUG & 2
        printf ("clrreset: nclr = 16\n");
#endif
        for (int i = 0; i < 16; ++i)
            {
            curpal[i] = defpal[i];
            }
        }
    }

void clrset (int pal, int phy, int r, int g, int b)
    {
    if ( phy < 16 ) curpal[pal] = defpal[phy];
    else if ( phy == 16 ) curpal[pal] = PIXEL_FROM_RGB8 (r, g, b);
    else if ( phy == 255 ) curpal[pal] = PIXEL_FROM_RGB8 (8*r, 8*g, 8*b);
#if DEBUG & 2
    printf ("curpal[%d] = 0x%04X\n", pal, curpal[pal]);
#endif
    }

bool setmode (int mode, uint8_t **pfbuf, MODE **ppmd)
    {
#if DEBUG & 1
    printf ("setmode (%d)\n", mode);
#endif
    if (( mode >= 0 ) && ( mode < sizeof (modes) / sizeof (modes[0]) ))
        {
        memcpy (&curmode, &modes[mode], sizeof (MODE));
        if (curmode.ncbt == 3) xscl = 1;
        else                   xscl = (curmode.ncbt * curmode.nppb) >> 3;
        yscl = 1 << curmode.yshf;
        // nrow = curmode.grow << curmode.yshf;
        // ILI9488 has 480 rows of graphics memory
        nrow = 480;
        // printf ("xscl = %d, yscl = %d, nrow = %d, vmgn = %d\n", xscl, yscl, nrow, curmode.vmgn);
        scrltop = 0;
        LCD_ScrollArea (curmode.vmgn, nrow, curmode.vmgn);
        LCD_Scroll (curmode.vmgn + scrltop);
        if (curmode.vmgn > 0)
            {
            LCD_SetAreaColour (0, 0, curmode.gcol * xscl, curmode.vmgn, 0);
            LCD_SetAreaColour (0, nrow + curmode.vmgn, curmode.gcol * xscl, nrow + 2 * curmode.vmgn, 0);
            }
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

static void pclcd_ttx (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2, bool bShow)
    {
    // printf ("pclcd_out7 (%p, %d, %d, %d, %d)\n", fbuf, xp1, yp1, xp2, yp2);
    if (fbuf != framebuf) return;
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
                LCD_Data_Init ();
                // printf ("LCD_WriteColour:");
                for (int ys = 0; ys < TTH; ++ys)
                    {
                    if (bInverse && (bFlash ||
                                    ((xc == xcsr) && (yr == ycsr)
                                    && (ys >= csrtop) && (ys < csrtop + csrhgt)
                                    && (nCsrHide == 0))))
                        {
                        LCD_WriteColour (bgnd, 8 * xscl * yscl);
                        // printf (" %0x%02X * %d", bgnd, 8 * xscl);
                        }
                    else if (bShow || bFlash)
                        {
                        for (int y = 0; y < yscl; ++y)
                            {
                            for (int i = 0; i < 8; ++i)
                                {
                                LCD_WriteColour (blk[ys][i], xscl);
                                // printf (" 0x%02X", blk[i]);
                                }
                            }
                        // printf ("\n");
                        }
                    }
                LCD_Data_Term ();
                }
            ++pch;
            }
        if (bDHeight) bLower = ! bLower;
        else bLower = false;
        }
    }

void pclcd_out7 (uint8_t *fbuf, int xp1, int yp1, int xp2, int yp2)
    {
    pclcd_ttx (fbuf, xp1, yp1, xp2, yp2, true);
    }

void pclcd_scroll7 (uint8_t *fbuf, int lt, int lb, bool bUp)
    {
    if ((lt != 0) || (lb != curmode.trow - 1))
        {
        pclcd_out7 (fbuf, 0, lt, curmode.tcol - 1, lb);
        }
    else if (bUp)
        {
        scrltop += curmode.thgt << curmode.yshf;
        if ( scrltop >= nrow) scrltop -= nrow;
        LCD_Scroll (curmode.vmgn + scrltop);
        pclcd_out7 (fbuf, 0, curmode.trow - 1, curmode.tcol - 1, curmode.trow - 1);
        }
    else
        {
        scrltop -= curmode.thgt << curmode.yshf;
        if ( scrltop < 0) scrltop += nrow;
        LCD_Scroll (curmode.vmgn + scrltop);
        pclcd_out7 (fbuf, 0, 0, curmode.tcol - 1, 0);
        }
    }

void mode7flash (void)
    {
    pclcd_ttx (framebuf, 0, 0, curmode.tcol - 1, curmode.trow - 1, false);
    }

void gsize (uint32_t *pwth, uint32_t *phgt)
    {
    *pwth = PICO_LCD_COLUMNS;
    *phgt = PICO_LCD_ROWS;
    }
