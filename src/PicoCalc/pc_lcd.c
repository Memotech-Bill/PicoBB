/*  lcddrv.c - Implementation of the low level graphics routines for an LCD display with memory */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <pico/time.h>
#include <pico/sync.h>
#include "bbccon.h"
#include "vducmd.h"
#include "periodic.h"
#include "qspi.h"

#include "font_tt.h"

#ifndef RGB
#define RGB     16
#endif

#if RGB == 18
typedef uint32_t colour_t;
#elif RGB == 16
typedef uint16_t colour_t;
#else
#error Invalid colour bit size
#endif

#if REF_MODE & 1
#define RAMBLK  1024                // Block size of PSRAM
#define RAMBMSK (~(RAMBLK - 1))     // Mask to address start of block

static void load_lcd (void);
static void save_lcd (void);
static void save_pixels (colour_t *pix, int nPix);
static void save_colour (colour_t clr, int nClr);
static void save_scroll (int data);
static void load_pixels (colour_t *pix, int nPix);
static colour_t load_colour (void);
#endif

#if REF_MODE == 3
RFM rfm = rfmBuffer;
#endif

static void video_periodic (void);

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

static const MODE modes[] = {
// ncbt gcol grow tcol trw  vmg hmg pb nbpl ys thg
    { 1, 320, 256,  40, 32,  32,  0,  0, 0, 0,  8},   // Mode  0
    { 2, 320, 256,  40, 32,  32,  0,  0, 0, 0,  8},   // Mode  1
    { 4, 160, 256,  20, 32,  32,  0,  0, 0, 0,  8},   // Mode  2
    { 1, 320, 225,  40, 25,  47,  0,  0, 0, 0,  9},   // Mode  3
    { 1, 320, 256,  40, 32,  32,  0,  0, 0, 0,  8},   // Mode  4
    { 2, 160, 256,  20, 32,  32,  0,  0, 0, 0,  8},   // Mode  5
    { 1, 320, 225,  40, 25,  47,  0,  0, 0, 0,  9},   // Mode  6
    { 3, 320, 225,  40, 25,  47,  0,  0, 0, 0,TTH},   // Mode  7 - Teletext
    { 4, 320, 320,  40, 20,   0,  0,  0, 0, 0, 16},   // Mode  8
    { 4, 320, 320,  40, 32,   0,  0,  0, 0, 0, 10},   // Mode  9
    { 4, 320, 320,  40, 40,   0,  0,  0, 0, 0,  8},   // Mode  10
    { 7, 320, 320,  40, 20,   0,  0,  0, 0, 0, 16},   // Mode  11
    { 7, 320, 320,  40, 32,   0,  0,  0, 0, 0, 10},   // Mode  12
    { 7, 320, 320,  40, 40,   0,  0,  0, 0, 0,  8}    // Mode  13
    };

#if RGB == 16
#define PIXEL_FROM_RGB8(r, g, b)    (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#elif RGB == 18
#define PIXEL_FROM_RGB8(r, g, b)    (((r & 0xFC) << 16) | ((g & 0xFC) << 8) | (b & 0xFC))
#endif

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

static colour_t curpal[128];

typedef struct
    {
    colour_t    clr;
    uint8_t     idx;
    } CLRIDX;

static CLRIDX clridx[128];

static const MODE *pmode;
static int scrltop = 0;
static int xscl = 1;
static int yscl = 1;
static int nrow;

static bool bBlink = false;

static bool bCsrVis = false;
static int nCsrHide = 0;
static uint32_t nFlash = 0;             // Time counter for cursor flash
static critical_section_t cs_csr;       // Critical section controlling cursor flash
static PRD_FUNC pnext = NULL;

#if REF_MODE & 1
static int nRowBeg = 0;
static int nRowEnd = SWIDTH;
static int nColBeg = 0;
static int nColEnd = SHEIGHT;
static int nRowCur = 0;
static int nColCur = 0;
static bool bBuffer = false;
#endif

static void LCD_SPI_Init (void)
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
    // gpio_set_function (PICO_LCD_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function (PICO_LCD_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled (PICO_LCD_RX_PIN, true);
    int speed = spi_init (SPI_INSTANCE(PICO_LCD_SPI), PICO_LCD_SPEED);
    // printf ("LCD speed = %d\n", speed);
    }

static inline void LCD_WriteReg (unsigned char data)
    {
    gpio_set_function (PICO_LCD_TX_PIN, GPIO_FUNC_SPI);
    gpio_put (PICO_LCD_DC_PIN, 0);
    gpio_put (PICO_LCD_CS_PIN, 0);

    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), &data, 1);

    // gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void LCD_WriteData8 (uint8_t data)
    {
    gpio_put (PICO_LCD_DC_PIN, 1);
    // gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), &data, 1);
    // gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void LCD_WriteData16 (uint16_t data)
    {
    uint8_t data_array[2];
    data_array[0] = (data >> 8) & 0xFF;
    data_array[1] = data & 0xFF;

    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    // gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data_array, 2);
    // gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void LCD_WriteData24 (uint32_t data)
    {
    uint8_t data_array[3];
    data_array[0] = (data >> 16) & 0xFF;
    data_array[1] = (data >> 8) & 0xFF;
    data_array[2] = data & 0xFF;


    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    // gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data_array, 3);
    // gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void LCD_WriteData (uint8_t *data, int nLen)
    {
    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    // gpio_put (PICO_LCD_CS_PIN, 0);
    spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data, nLen);
    // gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void LCD_ReadData (uint8_t *data, int nLen)
    {
    gpio_set_function (PICO_LCD_TX_PIN, GPIO_FUNC_SPI);
    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    // gpio_put (PICO_LCD_CS_PIN, 0);
    spi_read_blocking (SPI_INSTANCE(PICO_LCD_SPI), 0, data, nLen);
    // gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void LCD_DataInput (void)
    {
    LCD_WriteReg (0x2E);
    gpio_init (PICO_LCD_TX_PIN);
    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    // gpio_put (PICO_LCD_CS_PIN, 0);
    uint8_t data;
    spi_read_blocking (SPI_INSTANCE(PICO_LCD_SPI), 0, &data, 1);
    }

static inline void Dsp_DataInput (void)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        qspi_cfg_qread ();
        return;
        }
#endif
    LCD_DataInput ();
    }

static inline void LCD_DataOutput (void)
    {
    LCD_WriteReg (0x2C);
    gpio_put (PICO_LCD_DC_PIN, 1); // Data mode
    // gpio_put (PICO_LCD_CS_PIN, 0);
    }

static inline void Dsp_DataOutput (void)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        // printf ("Dsp_DataOutput:\n");
        qspi_cfg_qwrite ();
        return;
        }
#endif
    LCD_DataOutput ();
    }

static inline void LCD_DataTerm (void)
    {
    gpio_put (PICO_LCD_CS_PIN, 1);
    }

static inline void Dsp_DataTerm (void)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        qspi_free ();
        return;
        }
#endif
    LCD_DataTerm ();
    }

static void LCD_WriteColour (colour_t clr, int nRpt)
    {
#if RGB == 18
    uint8_t data[3];
    data[0] = (clr >> 16) & 0xFF;
    data[1] = (clr >> 8) & 0xFF;
    data[2] = clr & 0xFF;
    while (nRpt)
        {
        spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data, 3);
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

static void Dsp_WriteColour (colour_t clr, int nRpt)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        save_colour (clr, nRpt);
        return;
        }
#endif
    LCD_WriteColour (clr, nRpt);
    }

static colour_t LCD_ReadColour (void)
    {
    colour_t clr;
#if RGB == 18
    uint8_t data[3];
    spi_read_blocking (SPI_INSTANCE(PICO_LCD_SPI), 0, data, 3);
    clr = (data[0] << 16) | (data[1] << 8) | data[2];
#else
    uint8_t data[2];
    spi_read_blocking (SPI_INSTANCE(PICO_LCD_SPI), 0, data, 2);
    clr = (data[0] << 8) | data[1];
#endif
    return clr;
    }

static colour_t Dsp_ReadColour (void)
    {
#if REF_MODE & 1
    if (bBuffer) return load_colour ();
#endif
    return LCD_ReadColour ();
    }

static void LCD_WritePixels (colour_t *pix, int nPix)
    {
    while (nPix)
        {
#if RGB == 18
        uint8_t data[3];
        data[0] = (*pix >> 16) & 0xFF;
        data[1] = (*pix >> 8) & 0xFF;
        data[2] = *pix & 0xFF;
        spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data, 3);
#else
        uint8_t data[2];
        data[0] = (*pix >> 8) & 0xFF;
        data[1] = *pix & 0xFF;
        spi_write_blocking (SPI_INSTANCE(PICO_LCD_SPI), data, 2);
#endif
        ++pix;
        --nPix;
        }
    }

static void Dsp_WritePixels (colour_t *pix, int nPix)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        save_pixels (pix, nPix);
        return;
        }
#endif
    LCD_WritePixels (pix, nPix);
    }

static void LCD_ReadPixels (colour_t *pix, int nPix)
    {
    while (nPix)
        {
#if RGB == 18
        uint8_t data[3];
        spi_read_blocking (SPI_INSTANCE(PICO_LCD_SPI), 0, data, 3);
        *pix = (data[0] << 16) | (data[1] << 8) | data[2];
#else
        uint8_t data[2];
        spi_read_blocking (SPI_INSTANCE(PICO_LCD_SPI), 0, data, 2);
        *pix = (data[0] << 8) | data[1];
#endif
        ++pix;
        --nPix;
        }
    }

static void Dsp_ReadPixels (colour_t *pix, int nPix)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        load_pixels (pix, nPix);
        return;
        }
#endif
    LCD_ReadPixels (pix, nPix);
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
    LCD_DataTerm ();

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
    LCD_DataTerm ();

    LCD_WriteReg (0XC0); // Power Control 1
    LCD_WriteData8 (0x17);
    LCD_WriteData8 (0x15);
    LCD_DataTerm ();

    LCD_WriteReg (0xC1); // Power Control 2
    LCD_WriteData8 (0x41);
    LCD_DataTerm ();

    LCD_WriteReg (0xC5); // VCOM Control
    LCD_WriteData8 (0x00);
    LCD_WriteData8 (0x12);
    LCD_WriteData8 (0x80);
    LCD_DataTerm ();

    LCD_WriteReg (0x36); // Memory Access Control
    LCD_WriteData8 (0x48); // MX, BGR
    LCD_DataTerm ();

    LCD_WriteReg (0x3A); // Pixel Interface Format
#if RGB == 18
    LCD_WriteData8 (0x66); // 18 bit colour for SPI
#else
    // LCD_WriteData8 (0x55); // 16 bit colour for SPI
    LCD_WriteData8 (0x65); // 16 bit colour for SPI
#endif
    LCD_DataTerm ();
    
    LCD_WriteReg (0xB0); // Interface Mode Control
    LCD_WriteData8 (0x00);
    LCD_DataTerm ();

    LCD_WriteReg (0xB1); // Frame Rate Control
    LCD_WriteData8 (0xA0);
    LCD_DataTerm ();

    LCD_WriteReg (0x21); // Display Inversion On
    LCD_DataTerm ();

    LCD_WriteReg (0xB4); // Display Inversion Control
    LCD_WriteData8 (0x02);
    LCD_DataTerm ();

    LCD_WriteReg (0xB6); // Display Function Control
    LCD_WriteData8 (0x02);
    LCD_WriteData8 (0x02);
    LCD_WriteData8 (0x3B);
    LCD_DataTerm ();

    LCD_WriteReg (0xB7); // Entry Mode Set
    LCD_WriteData8 (0xC6);
    LCD_DataTerm ();
    
    LCD_WriteReg (0xE9); // Set image function
#if RGB == 18
    LCD_WriteData8 (0x01);    // 1 = Enable 24-bit input
#else
    LCD_WriteData8 (0x00);    // 1 = Enable 24-bit input
#endif
    LCD_DataTerm ();

    LCD_WriteReg (0xF7); // Adjust Control 3 - Is this required?
    LCD_WriteData8 (0xA9);
    LCD_WriteData8 (0x51);
    LCD_WriteData8 (0x2C);
    LCD_WriteData8 (0x82);    // DSI write DCS command, use loose packet RGB 666
    LCD_DataTerm ();

    LCD_WriteReg (0x11); //Exit Sleep
    LCD_DataTerm ();
    sleep_ms (120);

    LCD_WriteReg (0x29); //Display on
    LCD_DataTerm ();
    sleep_ms (120);

    LCD_WriteReg (0x36); // Memory Access Control
    LCD_WriteData8 (0x48); // MX, BGR
    LCD_DataTerm ();

    critical_section_init (&cs_csr);
    pnext = add_periodic (video_periodic);

    modechg (8);
    }

void LCD_Scroll (int data)
    {
    LCD_WriteReg (0x37);
    LCD_WriteData16 (data);
    LCD_DataTerm ();
    }

void Dsp_Scroll (int data)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        save_scroll (data);
        return;
        }
#endif
    LCD_Scroll (data);
    }

void LCD_ScrollArea (int top, int mid, int bot)
    {
    LCD_WriteReg (0x33);
    LCD_WriteData16 (top);
    LCD_WriteData16 (mid);
    LCD_WriteData16 (bot);
    LCD_DataTerm ();
    }

void LCD_SetWindow (int Xstart, int Ystart,	int Xend, int Yend)
    {
	//set the X coordinates
	LCD_WriteReg(0x2A);
	LCD_WriteData16 (Xstart);
	LCD_WriteData16 (Xend - 1);
    LCD_DataTerm ();

	//set the Y coordinates
	LCD_WriteReg(0x2B);
	LCD_WriteData16 (Ystart);
	LCD_WriteData16 (Yend - 1);
    LCD_DataTerm ();
    }

void Dsp_SetWindow (int Xstart, int Ystart,	int Xend, int Yend)
    {
#if REF_MODE & 1
    if (bBuffer)
        {
        nColCur = Xstart;
        nColBeg = Xstart;
        nColEnd = Xend;
        nRowCur = Ystart;
        nRowBeg = Ystart;
        nRowEnd = Yend;
        return;
        }
#endif
    LCD_SetWindow (Xstart, Ystart, Xend, Yend);
    }

void LCD_SetAreaColour (int Xstart, int Ystart, int Xend, int Yend,	uint32_t Colour)
    {
    if((Xend > Xstart) && (Yend > Ystart))
        {
        LCD_SetWindow (Xstart, Ystart, Xend, Yend);
        LCD_DataOutput ();
        LCD_WriteColour (Colour, (uint32_t)(Xend - Xstart) * (uint32_t)(Yend - Ystart));
        LCD_DataTerm ();
        }
    }

void Dsp_SetAreaColour (int Xstart, int Ystart, int Xend, int Yend,	uint32_t Colour)
    {
    if((Xend > Xstart) && (Yend > Ystart))
        {
        Dsp_SetWindow (Xstart, Ystart, Xend, Yend);
        // printf ("Dsp_SetAreaColour:\n");
        Dsp_DataOutput ();
        Dsp_WriteColour (Colour, (uint32_t)(Xend - Xstart) * (uint32_t)(Yend - Ystart));
        Dsp_DataTerm ();
        }
    }

void Dsp_Clear (uint32_t Colour)
    {
    Dsp_SetAreaColour (0, 0, SWIDTH, SHEIGHT, Colour);
    }

static void idxclr (void)
    {
    int nclr = 1 << pmode->ncbt;
    for (int i = 0; i < nclr; ++i)
        {
        clridx[i].clr = curpal[i];
        clridx[i].idx = i;
        }
    bool bSorted = false;
    while (! bSorted)
        {
        bSorted = true;
        for (int i = 1; i < nclr; ++i)
            {
            if (clridx[i].clr < clridx[i-1].clr)
                {
                bSorted = false;
                colour_t clr = clridx[i].clr;
                uint8_t idx = clridx[i].idx;
                clridx[i].clr = clridx[i-1].clr;
                clridx[i].idx = clridx[i-1].idx;
                clridx[i-1].clr = clr;
                clridx[i-1].idx = idx;
                }
            }
        }
    }

static uint8_t findclr (colour_t clr)
    {
    int n1 = -1;
    int n2 = 1 << pmode->ncbt;
    while (n2 > n1 + 1)
        {
        int n3 = (n1 + n2) / 2;
        if (clr == clridx[n3].clr)      return clridx[n3].idx;
        else if (clr < clridx[n3].clr)  n2 = n3;
        else                            n1 = n3;
        }
    return 0;
    }

colour_t rgbclr (int r, int g, int b)
    {
#if RGB == 16
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
#elif RGB == 18
    return ((r & 0xFC) << 16) | ((g & 0xFC) << 8) | (b & 0xFC);
#endif
    }

int clrrgb (int clr)
    {
    clr = curpal[clr];
#if RGB == 16
    return ((clr & 0xF800) >> 8) | ((clr & 0x07E0) << 5) | ((clr & 0x001F) << 19);
#elif RGB == 18
    return ((clr & 0xFC0000) >> 16) | (clr & 0xFC00) | ((clr & 0x00FC) << 16);
#endif
    }

void clrreset (void)
    {
    if ( pmode->ncbt == 1 )
        {
#if DEBUG & 2
        // printf ("clrreset: nclr = 2\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[15];
        }
    else if ( pmode->ncbt == 2 )
        {
#if DEBUG & 2
        // printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        curpal[1] = defpal[9];
        curpal[2] = defpal[11];
        curpal[3] = defpal[15];
        }
    else if ( pmode->ncbt == 3 )
        {
#if DEBUG & 2
        // printf ("clrreset: nclr = 4\n");
#endif
        curpal[0] = defpal[0];
        for (int i = 1; i < 8; ++i)
            {
            curpal[i] = defpal[i+8];
            }
        }
    else if ( pmode->ncbt == 4 )
        {
#if DEBUG & 2
        // printf ("clrreset: nclr = 16\n");
#endif
        for (int i = 0; i < 16; ++i)
            {
            curpal[i] = defpal[i];
            }
        }
    else if ( pmode->ncbt == 7 )
        {
        for (int i = 0; i < 128; ++i)
            {
            curpal[i] = rgbclr (0x55 * (i & 0x03), 0x09 * (i & 0x1C), 0x55 * (i >> 5));
            }
        }
    }

void clrset (int pal, int phy, int r, int g, int b)
    {
    if ( phy < 16 ) curpal[pal] = defpal[phy];
    else if ( phy == 16 ) curpal[pal] = rgbclr (r, g, b);
    else if ( phy == 255 ) curpal[pal] = rgbclr (8*r, 8*g, 8*b);
#if DEBUG & 2
    // printf ("curpal[%d] = 0x%04X\n", pal, curpal[pal]);
#endif
    }

const MODE *setmode (int mode)
    {
#if DEBUG & 1
    // printf ("setmode (%d)\n", mode);
#endif
    if (( mode >= 0 ) && ( mode < sizeof (modes) / sizeof (modes[0]) ))
        {
        pmode = &modes[mode];
        xscl = 1;
        yscl = 1 << pmode->yshf;
        // ILI9488 has 480 rows of graphics memory
        nrow = SDEPTH - 2 * pmode->vmgn;
        scrltop = 0;
        LCD_ScrollArea (pmode->vmgn, nrow, pmode->vmgn);
        LCD_Scroll (pmode->vmgn + scrltop);
        if (pmode->vmgn > 0)
            {
            LCD_SetAreaColour (0, 0, pmode->gcol * xscl, pmode->vmgn, 0);
            LCD_SetAreaColour (0, nrow + pmode->vmgn, pmode->gcol * xscl, nrow + 2 * pmode->vmgn, 0);
            }
        return pmode;
        }
    return NULL;
    }

void dispenable (void)
    {
    nCsrHide = 0;
    showcsr ();
#if DEBUG & 1
    // printf ("dispmode: New mode set\n");
#endif
    }

void dispdn (void)
    {
    int thgt = pmode->thgt << pmode->yshf;
    int nC1 = 8 * xscl * tvl;
    int nC2 = 8 * xscl * (tvr + 1);
    if ((tvl == 0) && (tvr == pmode->tcol - 1) && (tvt == 0) && (tvb == pmode->trow - 1))
        {
        scrltop -= thgt;
        if ( scrltop < 0) scrltop += nrow;
        Dsp_Scroll (pmode->vmgn + scrltop);
        }
    else
        {
        colour_t pixbuf[pmode->gcol];
        int nR1 = pmode->vmgn + scrltop + thgt * tvb;
        int nR2 = nR1 - thgt;
        int nCol = nC2 - nC1;
        int nWrap = pmode->vmgn + nrow;
        for (int iRow = 0; iRow < thgt * (tvb - tvt); ++iRow)
            {
            --nR1;
            --nR2;
            if (nR1 == pmode->vmgn) nR1 = nWrap;
            if (nR2 == pmode->vmgn) nR2 = nWrap;
            Dsp_SetWindow (nC1, nR2, nC2, nR2 + 1);
            Dsp_DataInput ();
            Dsp_ReadPixels (pixbuf, nCol);
            Dsp_DataTerm ();
            Dsp_SetWindow (nC1, nR1, nC2, nR1 + 1);
            // printf ("dispdn:\n");
            Dsp_DataOutput ();
            Dsp_WritePixels (pixbuf, nCol);
            Dsp_DataTerm ();
            }
        }
    int iRow = pmode->vmgn + scrltop + tvt * thgt;
    if (iRow + thgt > nrow)
        {
        Dsp_SetAreaColour (nC1, iRow, nC2, pmode->vmgn + nrow, curpal[txtbak]);
        Dsp_SetAreaColour (nC1, pmode->vmgn, nC2, iRow + thgt - nrow, curpal[txtbak]);
        }
    else
        {
        Dsp_SetAreaColour (nC1, iRow, nC2, iRow + thgt, curpal[txtbak]);
        }
    }

void dispup (void)
    {
    int thgt = pmode->thgt << pmode->yshf;
    int nC1 = 8 * xscl * tvl;
    int nC2 = 8 * xscl * (tvr + 1);
    if ((tvl == 0) && (tvr == pmode->tcol - 1) && (tvt == 0) && (tvb == pmode->trow - 1))
        {
        scrltop += thgt;
        if ( scrltop >= nrow) scrltop -= nrow;
        Dsp_Scroll (pmode->vmgn + scrltop);
        }
    else
        {
        uint16_t pixbuf[2 * pmode->gcol];
        int nR1 = pmode->vmgn + scrltop + thgt * tvt;
        int nR2 = nR1 + thgt;
        int nCol = nC2 - nC1;
        int nWrap = pmode->vmgn + nrow;
        for (int iRow = 0; iRow < thgt * (tvb - tvt); ++iRow)
            {
            if (nR1 == nWrap) nR1 = pmode->vmgn;
            if (nR2 == nWrap) nR2 = pmode->vmgn;
            Dsp_SetWindow (nC1, nR2, nC2, nR2 + 1);
            Dsp_DataInput ();
            Dsp_ReadPixels (pixbuf, nCol);
            Dsp_DataTerm ();
            Dsp_SetWindow (nC1, nR1, nC2, nR1 + 1);
            // printf ("dispup:\n");
            Dsp_DataOutput ();
            Dsp_WritePixels (pixbuf, nCol);
            Dsp_DataTerm ();
            ++nR1;
            ++nR2;
            }
        }
    int iRow = pmode->vmgn + scrltop + tvb * thgt;
    if (iRow >= nrow) iRow -= nrow;
    if (iRow + thgt > nrow)
        {
        Dsp_SetAreaColour (nC1, iRow, nC2, pmode->vmgn + nrow, curpal[txtbak]);
        Dsp_SetAreaColour (nC1, pmode->vmgn, nC2, iRow + thgt - nrow, curpal[txtbak]);
        }
    else
        {
        Dsp_SetAreaColour (nC1, iRow, nC2, iRow + thgt, curpal[txtbak]);
        }
    }

void cls (void)
    {
    int thgt = pmode->thgt << pmode->yshf;
    int nC1 = 8 * xscl * tvl;
    int nC2 = 8 * xscl * (tvr + 1);
    int nR1 = scrltop + tvt * thgt;
    int nR2 = scrltop + (tvb + 1) * thgt;
    if (nR1 >= nrow) nR1 -= nrow;
    if (nR2 >= nrow) nR2 -= nrow;
    nR1 += pmode->vmgn;
    nR2 += pmode->vmgn;
    if (nR2 < nR1)
        {
        Dsp_SetAreaColour (nC1, nR1, nC2, pmode->vmgn + nrow, curpal[txtbak]);
        Dsp_SetAreaColour (nC1, pmode->vmgn, nC2, nR2, curpal[txtbak]);
        }
    else
        {
        Dsp_SetAreaColour (nC1, nR1, nC2, nR2, curpal[txtbak]);
        }
    }

typedef enum {dhNone, dhUpper, dhLower} DBLHGT;

typedef struct
    {
    uint16_t    ttd[32];
    uint8_t     ch[32];
    uint32_t    iChg;
    DBLHGT      dh;
    } TTX_ROW;

static TTX_ROW ttx_disp[26];

void ttx_scanrow (TTX_ROW *pttx, DBLHGT dh)
    {
    pttx->iChg = 0;
    int bg = 0;
    int fg = 7 << 12;
    pttx->dh = dhNone;
    bool bDblHgt = false;
    bool bFlash = false;
    bool bGraph = false;
    bool bCont = true;
    bool bHold = false;
    bool bHide = false;
    uint16_t ttd;
    for (int i = 0; i < pmode->tcol; ++i)
        {
        uint8_t ch = pttx->ch[i];
        if (ch > 0x20)
            {
            if (bHide)
                {
                ttd = bg;
                }
            else if (bGraph)
                {
                ttd = (ch + (bCont ? 0xA0 : 0x40)) | bg | fg | (bFlash ? 0x8000 : 0x00);
                }
            else
                {
                ttd = (ch + (bDblHgt ? 0x100 : 0x00)) | bg | fg | (bFlash ? 0x8000 : 0x00);
                }
            }
        else if (ch == 0x20)
            {
            ttd = bg;
            }
        else
            {
            bHide = false;
            if (ch < 0x08)
                {
                bGraph = false;
                fg = ch << 12;
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
                bDblHgt = false;
                }
            else if (ch == 0x0D)
                {
                pttx->dh = (dh == dhUpper) ? dhLower : dhUpper;
                bDblHgt = true;
                }
            else if ((ch >= 0x10) && (ch <= 0x17))
                {
                bGraph = true;
                fg = (ch & 0x07) << 12;
                }
            else if (ch == 0x18)
                {
                bHide = true;
                }
            else if (ch == 0x19)
                {
                bCont = true;
                }
            else if (ch == 0x1A)
                {
                bCont = false;
                }
            else if (ch == 0x1C)
                {
                bg = 0;
                }
            else if (ch == 0x1D)
                {
                bg = fg >> 3;
                }
            else if (ch == 0x1E)
                {
                bHold = true;
                }
            else if (ch == 0x1F)
                {
                bHold = false;
                }
            if (! bHold) ttd = bg;
            }
        pttx->iChg <<= 1;
        if (ttd != pttx->ttd[i])
            {
            pttx->ttd[i] = ttd;
            pttx->iChg |= 1;
            }
        }
    }

void ttx_disprow (const TTX_ROW *pttx, int iRow)
    {
    int thgt = pmode->thgt << pmode->yshf;
    int nR1 = scrltop + thgt * iRow;
    if (nR1 >= nrow) nR1 -= nrow;
    nR1 += pmode->vmgn;
    int nR2 = nR1 + thgt;
    uint32_t iChg = pttx->iChg;
    for (int iCol = 0; iCol < pmode->tcol; ++iCol)
        {
        if (iChg & 0x80000000)
            {
            bool bDblHgt = false;
            uint16_t ttd = pttx->ttd[iCol];
            uint16_t ch = ttd & 0x1FF;
            if (ch >= 0x120)
                {
                ch -= 0x120;
                bDblHgt = true;
                }
            int nC1 = 8 * xscl * iCol;
            int nC2 = nC1 + 8 * xscl;
            
            colour_t bg = curpal[(ttd & 0x0E00) >> 8];
            colour_t fg = curpal[ttd >> 12];
            if (bBlink)
                {
                if ((iCol == xcsr) && (iRow == ycsr))
                    {
                    colour_t tc = bg;
                    bg = fg;
                    fg = tc;
                    }
                else if (ttd & 0x8000)
                    {
                    fg = bg;
                    }
                }
            Dsp_SetWindow (nC1, nR1, nC2, nR2);
            // printf ("ttx_disprow:\n");
            Dsp_DataOutput ();
            for (int iScan = 0; iScan < pmode->thgt; ++iScan)
                {
                int iFRow = iScan >> pmode->yshf;
                if (bDblHgt)
                    {
                    if (pttx->dh == dhLower) iFRow += TTH;
                    iFRow /= 2;
                    }
                uint8_t pix = font_tt[ch][iFRow];
                for (int j = 0; j < 8; ++j)
                    {
                    if (pix & 0x80) Dsp_WriteColour (fg, xscl);
                    else Dsp_WriteColour (bg, xscl);
                    pix <<= 1;
                    }
                }
            Dsp_DataTerm ();
            }
        iChg <<= 1;
        }
    }

void disp_ttx (char chr)
    {
    ttx_disp[ycsr].ch[xcsr] = chr & 0x7F;
    DBLHGT dh = dhNone;
    if (ycsr > 0) dh = ttx_disp[ycsr-1].dh;
    int iRow = ycsr;
    while (true)
        {
        ttx_scanrow (&ttx_disp[iRow], dh);
        if (ttx_disp[iRow].iChg == 0) break;
        ttx_disprow (&ttx_disp[iRow], iRow);
        dh = ttx_disp[iRow].dh;
        if (iRow == pmode->trow) break;
        ++iRow;
        if (ttx_disp[iRow].dh == dhNone)
            {
            break;
            }
        else if (ttx_disp[iRow].dh == dhUpper)
            {
            if (dh != dhUpper) break;
            }
        else
            {
            if (dh == dhUpper) break;
            }
        }
    }

static void mode7flash (void)
    {
    bBlink = ! bBlink;
    for (int iRow = 0; iRow < pmode->trow; ++iRow)
        {
        TTX_ROW *pttx = &ttx_disp[iRow];
        uint32_t iSave = pttx->iChg;
        pttx->iChg = 0;
        for (int iCol = 0; iCol < pmode->tcol; ++iCol)
            {
            pttx->iChg <<= 1;
            if (pttx->ttd[iCol] & 0x8000) pttx->iChg |= 1;
            if ((iCol == xcsr) && (iRow == ycsr)) pttx->iChg |= 1;
            }
        ttx_disprow (pttx, iRow);
        pttx->iChg = iSave;
        }
    }

void scrldn (void)
    {
    int savebg = txtbak;
    hidecsr ();
    if (pmode->ncbt == 3)
        {
        for (int iRow = tvb; iRow > tvt; --iRow)
            {
            memcpy (&ttx_disp[iRow].ttd[tvl], &ttx_disp[iRow-1].ttd[tvl], 2 * (tvr - tvl + 1));
            memcpy (&ttx_disp[iRow].ch[tvl], &ttx_disp[iRow-1].ch[tvl], tvr - tvl + 1);
            }
        uint16_t bg = 0;
        if (tvl > 0) bg = ttx_disp[tvt].ttd[tvl-1] & 0xFE00;
        for (int iCol = tvl; iCol <= tvr; ++iCol)
            {
            ttx_disp[tvt].ttd[iCol] = bg;
            ttx_disp[tvt].ch[iCol] = ' ';
            }
        txtbak = (bg >> 9) & 0x07;
        }
    dispdn ();
    txtbak = savebg;
    showcsr ();
    }

void scrlup (void)
    {
    int savebg = txtbak;
    hidecsr ();
    if (pmode->ncbt == 3)
        {
        for (int iRow = tvt; iRow < tvb; ++iRow)
            {
            memcpy (&ttx_disp[iRow].ttd[tvl], &ttx_disp[iRow+1].ttd[tvl], 2 * (tvr - tvl + 1));
            memcpy (&ttx_disp[iRow].ch[tvl], &ttx_disp[iRow+1].ch[tvl], tvr - tvl + 1);
            }
        uint16_t bg = 0;
        if (tvl > 0) bg = ttx_disp[tvb].ttd[tvl-1] & 0xFE00;
        for (int iCol = tvl; iCol <= tvr; ++iCol)
            {
            ttx_disp[tvb].ttd[iCol] = bg;
            ttx_disp[tvb].ch[iCol] = ' ';
            }
        txtbak = (bg >> 9) & 0x07;
        }
    dispup ();
    txtbak = savebg;
    showcsr ();
    }

void disp_glyph (uint8_t *pch)
    {
    int thgt = pmode->thgt << pmode->yshf;
    int yshf = pmode->yshf;
    if (thgt > 8) ++yshf;
    int nR1 = scrltop + thgt * ycsr;
    if (nR1 >= nrow) nR1 -= nrow;
    nR1 += pmode->vmgn;
    int nR2 = nR1 + thgt;
    int nC1 = 8 * xscl * xcsr;
    int nC2 = nC1 + 8 * xscl;
    uint16_t bg = curpal[txtbak];
    uint16_t fg = curpal[txtfor];
    Dsp_SetWindow (nC1, nR1, nC2, nR2);
    // printf ("disp_glyph:\n");
    Dsp_DataOutput ();
    for (int iScan = 0; iScan < pmode->thgt; ++iScan)
        {
        uint8_t pix = pch[iScan >> yshf];
        for (int j = 0; j < 8; ++j)
            {
            // printf ("Row %d, Column %d\n", iScan, j);
            if (pix & 0x80) Dsp_WriteColour (fg, xscl);
            else Dsp_WriteColour (bg, xscl);
            pix <<= 1;
            }
        }
    Dsp_DataTerm ();
    }

static inline void pixop (int op, uint16_t *fb, uint16_t cpx)
    {
    switch (op)
        {
        case 0:
            *fb = cpx;
            break;
        case 1:
            *fb |= cpx;
            break;
        case 2:
            *fb &= cpx;
            break;
        case 3:
            *fb ^= cpx;
            break;
        case 4:
            *fb = ~(*fb);
            break;
        case 5:
            break;
        case 6:
            *fb &= ~ cpx;
            break;
        case 7:
            *fb |= ~ cpx;
            break;
        }
    }

void hline (int clrop, int xp1, int xp2, int yp)
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
    xp1 = xscl * xp1;
    xp2 = xscl * (xp2 + 1);
    yp = scrltop + (yp << pmode->yshf);
    if (yp >= nrow) yp -= nrow;
    uint16_t pix[pmode->gcol];
    for (int i = 0; i < (1 << pmode->yshf); ++i)
        {
        Dsp_SetWindow (xp1, yp, xp2, yp + 1);
        Dsp_DataInput ();
        Dsp_ReadPixels (pix, xp2 - xp1);
        uint16_t cpx = curpal[clrop & 0x7F];
        int op = clrop >> 8;
        for (int i = 0; i <= xp2 - xp1; ++i)
            pixop (op, &pix[i], cpx);
        Dsp_DataTerm ();
        // printf ("hline:\n");
        Dsp_DataOutput ();
        Dsp_WritePixels (pix, xp2 - xp1);
        Dsp_DataTerm ();
        ++yp;
        }
    }

void point (int clrop, uint32_t xp, uint32_t yp)
    {
    hline (clrop, xp, xp, yp);
    }

void clrgraph (void)
    {
    gvl = xscl * gvl;
    gvr = xscl * gvr;
    gvt = scrltop + (gvt << pmode->yshf);
    gvb = scrltop + (gvb << pmode->yshf);
    if (gvt >= nrow)
        {
        gvt -= nrow;
        gvb -= nrow;
        }
    if (gvb <= nrow)
        {
        Dsp_SetAreaColour (gvl, gvt, gvr, gvb, curpal[bakgnd]);
        }
    else
        {
        gvb -= nrow;
        Dsp_SetAreaColour (gvl, gvt, gvr, nrow, curpal[bakgnd]);
        Dsp_SetAreaColour (gvl, scrltop, gvr, gvb, curpal[bakgnd]);
        }
    }

static colour_t getcolour (int xp, int yp)
    {
    xp = xscl * xp;
    yp = scrltop + (yp << pmode->yshf);
    if (yp >= nrow) yp -= nrow;
    yp += pmode->vmgn;
    Dsp_SetWindow (xp, yp, xp + 1, yp + 1);
    Dsp_DataInput ();
    colour_t clr;
    Dsp_ReadPixels (&clr, 1);
    Dsp_DataTerm ();
    return clr;
    }

uint8_t getpix (int xp, int yp)
    {
    return findclr (getcolour (xp, yp));
    }

int get_ttx (int x, int y)
    {
    int chr = ttx_disp[y].ch[x];
    if ( chr < 0x20 ) chr += 0x80;
    return chr;
    }

void get_glyph (int x, int y, int bgclr, uint8_t *prow)
    {
    bgclr = curpal[bgclr];
    int fhgt = pmode->thgt;
    int ystep = 1 << pmode->yshf;
    if ( fhgt > 10 )
        {
        fhgt >> 1;
        ystep << 1;
        }
    fhgt = 8;
    x *= 8;
    y *= pmode->thgt << pmode->yshf;
    for (int j = 0; j < fhgt; ++j)
        {
        for (int i = 0; i < 8; ++i)
            {
            *prow <<= 1;
            if ( getpix (x+i, y) != bgclr ) *prow |= 0x01;
            }
        y += ystep;
        ++prow;
        }
    }

static void flipcsr (int xp, int yp)
    {
    yp += scrltop + cursa;
    colour_t pix[8];
    for (int i = 0; i < cursb - cursa + 1; ++i)
        {
        if (yp >= nrow) yp -= nrow;
        LCD_SetWindow (xp, yp, xp + 8, yp + 1);
        LCD_DataInput ();
        LCD_ReadPixels (pix, 8);
        LCD_DataTerm ();
        for (int j = 0; j < 8; ++j) pix[j] = ~ pix[j];
        LCD_DataOutput ();
        LCD_WritePixels (pix, 8);
        LCD_DataTerm ();
        ++yp;
        }
    bCsrVis = ! bCsrVis;
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
        if ( bCsrVis ) flipcsr (xp, yp);
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
    if ((nCsrHide == 0) && (! bBuffer))
        {
        if ( pmode->ncbt != 3 )
            {
            critical_section_enter_blocking (&cs_csr);
            if ( ! bCsrVis ) flipcsr (xp, yp);
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
    else if ((nCsrHide == 0) && (! bBuffer))
        {
        int xp;
        int yp;
        if (csrpos (&xp, &yp))
            {
            critical_section_enter_blocking (&cs_csr);
            flipcsr (xp, yp);
            critical_section_exit (&cs_csr);
            }
        }
    }

void enablecsr (bool bEnable)
    {
    if (bEnable)    nCsrHide &= ~CSR_OFF;
    else            nCsrHide |= CSR_OFF;
    }

static void video_periodic (void)
    {
    if ( ++nFlash >= 5 )
        {
        flashcsr ();
        nFlash = 0;
        }
    if (pnext) pnext ();
    }

void gsize (uint32_t *pwth, uint32_t *phgt)
    {
    *pwth = SWIDTH;
    *phgt = SHEIGHT;
    }

int sdump (FILE *fBmp)
    {
    int nWrite = fwrite ("BM", 1, 2, fBmp);
    if ( nWrite != 2 ) return 198;
    int nClr = 1 << pmode->ncbt;
    uint8_t pBuff[320];
    int *iBuff = (int *) pBuff;
    iBuff[0] = 54 + 4 * nClr + pmode->grow * pmode->gcol;   // File size
    iBuff[1] = 0;                                           // Reserved
    iBuff[2] = 54 + 4 * nClr;                               // Offset to pixel data
    iBuff[3] = 40;                                          // BITMAPINFOHEADER size
    iBuff[4] = pmode->gcol;                                 // Width in pixels
    iBuff[5] = pmode->grow;                                 // Height in pixels
    iBuff[6] = 0x80001;                                     // Number of colour planes (low word) + Number of bits per pixel (high word)
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
    for (int iRow = pmode->grow - 1; iRow >= 0 ; --iRow)
        {
        Dsp_SetWindow (0, iRow, pmode->gcol, iRow + 1);
        Dsp_DataInput ();
        for (int iCol = 0; iCol < pmode->gcol; ++iCol)
            {
            pBuff[iCol] = findclr (Dsp_ReadColour ());
            }
        Dsp_DataTerm ();
        nWrite = fwrite (pBuff, 1, pmode->gcol, fBmp);
        if ( nWrite != pmode->gcol ) return 198;
        }
    return 0;
    }

int sload (FILE *fBmp)
    {
    uint16_t iBM;
    int nRead = fread (&iBM, sizeof (iBM), 1, fBmp);
    if ((nRead != 1) || (iBM != 0x4D42)) return 255;
    int nClr = 1 << pmode->ncbt;
    uint8_t pBuff[320];
    int *iBuff = (int *) pBuff;
    nRead = fread (iBuff, sizeof (int), 13, fBmp);
    if ((nRead != 13) || (iBuff[3] != 40)) return 255;
    if ((iBuff[4] != pmode->gcol) || (iBuff[5] != pmode->grow)
        || (iBuff[6] != 0x80001) || (iBuff[11] != nClr)) return 25;
    int iOff = iBuff[2];
    if (iBuff[2] == 54 + 4 * nClr) iOff = -1;
    nRead = fread (iBuff, sizeof (int), nClr, fBmp);
    if (nRead != nClr) return 255;
    for (int iClr = 0; iClr < nClr; ++iClr)
        {
        clrset (iClr, 16, iBuff[iClr] & 0xFF, (iBuff[iClr] >> 8) & 0xFF, (iBuff[iClr] >> 16) & 0xFF);
        }
    if (iOff > 0) fseek (fBmp, iOff, SEEK_SET);
    for (int iRow = pmode->grow - 1; iRow >= 0 ; --iRow)
        {
        Dsp_SetWindow (0, iRow, pmode->gcol, iRow + 1);
        // printf ("sload:\n");
        Dsp_DataOutput ();
        nRead = fread (pBuff, 1, pmode->gcol, fBmp);
        if (nRead != pmode->gcol) return 255;
        for (int iCol = 0; iCol < pmode->gcol; ++iCol)
            {
            Dsp_WriteColour (curpal[pBuff[iCol]], 1);
            }
        Dsp_DataTerm ();
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

static void show_buf (uint32_t addr, int nByte, uint8_t *buffer)
    {
    while (nByte > 0)
        {
        printf ("%08X:", addr);
        for (int i = 0; i < 15; ++i)
            {
            if (nByte <= 1) break;
            printf (" %02X", *buffer);
            ++buffer;
            ++addr;
            --nByte;
            }
        printf (" %02X\n", *buffer);
        ++buffer;
        ++addr;
        --nByte;
        }
    printf ("\n");
    }

static void save_lcd (void)
    {
    uint8_t sbuf[2][RAMBLK];
    bool bWrite = false;
    int ibuf = 0;
    uint32_t laddr = 0;
    uint32_t raddr = 0;
    int npix = RAMBLK / sizeof (colour_t);
    // printf ("save_lcd\n");
    qspi_cfg_qwrite ();
    hidecsr ();
    LCD_SetWindow (0, 0, SWIDTH, SDEPTH);
    LCD_DataInput ();
    while (true)
        {
        if (laddr + npix > SWIDTH * SDEPTH) npix = SWIDTH * SDEPTH - laddr;
        LCD_ReadPixels ((colour_t *) sbuf[ibuf], npix);
        if (bWrite) qspi_wait ();
        // show_buf (raddr, npix * sizeof (colour_t), sbuf[ibuf]);
        qspi_qwrite (raddr, npix * sizeof (colour_t), sbuf[ibuf]);
        bWrite = true;
        laddr += npix;
        raddr += npix * sizeof (colour_t);
        if (laddr >= SWIDTH * SDEPTH) break;
        ibuf = 1 - ibuf;
        }
    LCD_DataTerm ();
    qspi_wait ();
    // printf ("Write scrltop\n");
    // show_buf (raddr, sizeof (scrltop), (uint8_t *) &scrltop);
    qspi_qwrite (raddr, sizeof (scrltop), (uint8_t *) &scrltop);
    qspi_wait ();
    qspi_free ();
    showcsr ();
    }

static void load_lcd (void)
    {
    uint8_t sbuf[2][RAMBLK];
    int ibuf = 0;
    uint32_t laddr = 0;
    uint32_t raddr = 0;
    int npix = RAMBLK / sizeof (colour_t);
    // printf ("load_lcd\n");
    qspi_cfg_qread ();
    hidecsr ();
    LCD_SetWindow (0, 0, SWIDTH, SDEPTH);
    LCD_DataOutput ();
    qspi_qread (raddr, RAMBLK, sbuf[ibuf]);
    // show_buf (raddr, RAMBLK, sbuf[ibuf]);
    while (true)
        {
        qspi_wait ();
        raddr += npix * sizeof (colour_t);
        if (raddr >= SWIDTH * SDEPTH * sizeof (colour_t)) break;
        if (raddr + npix > SWIDTH * SDEPTH * sizeof (colour_t)) npix = SWIDTH * SDEPTH - raddr / sizeof (colour_t);
        qspi_qread (raddr, npix * sizeof (colour_t), sbuf[1 - ibuf]);
        // show_buf (raddr, npix * sizeof (colour_t), sbuf[1 - ibuf]);
        LCD_WritePixels ((colour_t *) sbuf[ibuf], RAMBLK / sizeof (colour_t));
        laddr += RAMBLK / sizeof (colour_t);
        ibuf = 1 - ibuf;
        }
    // printf ("Read scrltop\n");
    qspi_qread (raddr, sizeof (scrltop), (uint8_t *) &scrltop);
    // show_buf (raddr, sizeof (scrltop), (uint8_t *) &scrltop);
    LCD_WritePixels ((colour_t *) sbuf[ibuf], npix);
    LCD_DataTerm ();
    qspi_wait ();
    LCD_Scroll (pmode->vmgn + scrltop);
    qspi_free ();
    showcsr ();
    }

static void save_pixels (colour_t *pix, int nPix)
    {
    uint8_t *pbyt = (uint8_t *) pix;
    // printf ("save_pixels (0x%08X, %d)\n", pix, nPix);
    while (nPix > 0)
        {
        int nCol = nColEnd - nColCur;
        if (nCol > nPix) nCol = nPix;
        uint32_t addr_1 = (SWIDTH * nRowCur + nColCur) * sizeof (colour_t);
        uint32_t addr_2 = addr_1 + nCol * sizeof (colour_t);
        while (addr_1 < addr_2)
            {
            int nByte = addr_2 - addr_1;
            if ((addr_2 & RAMBMSK) != (addr_1 & RAMBMSK)) nByte = RAMBLK + (addr_1 & RAMBMSK) - addr_1;
            qspi_qwrite (addr_1, nByte, pbyt);
            qspi_wait ();
            addr_1 += nByte;
            pbyt += nByte;
            }
        nPix -= nCol;
        nColCur += nCol;
        if (nColCur == nColEnd)
            {
            nColCur = nColBeg;
            ++nRowCur;
            if (nRowCur == nRowEnd) nRowCur = nRowBeg;
            }
        }
    }

static void save_colour (colour_t clr, int nClr)
    {
    // printf ("save_colour (0x%08X, %d)\n", clr, nClr);
    colour_t cbuf[RAMBLK / sizeof (colour_t)];
    int nPix = nClr;
    if (nPix > RAMBLK / sizeof (colour_t)) nPix = RAMBLK / sizeof (colour_t);
    for (int i = 0; i < nPix; ++i) cbuf[i] = clr;
    while (nClr > 0)
        {
        if (nPix > nClr) nPix = nClr;
        save_pixels (cbuf, nPix);
        nClr -= nPix;
        }
    }

static void save_scroll (int data)
    {
    uint32_t addr = SWIDTH * SDEPTH * sizeof (colour_t);
    // printf ("save_scroll\n");
    qspi_cfg_qwrite ();
    qspi_qwrite (addr, sizeof (scrltop), (uint8_t *) &data);
    qspi_wait ();
    qspi_free ();
    }

static void load_pixels (colour_t *pix, int nPix)
    {
    uint8_t *pbyt = (uint8_t *) pix;
    // printf ("load_pixels\n");
    while (nPix > 0)
        {
        int nCol = nColEnd - nColCur;
        if (nCol > nPix) nCol = nPix;
        uint32_t addr_1 = (SWIDTH * nRowCur + nColCur) * sizeof (colour_t);
        uint32_t addr_2 = addr_1 + nCol * sizeof (colour_t);
        while (addr_1 < addr_2)
            {
            int nByte = addr_2 - addr_1;
            if ((addr_2 & RAMBMSK) != (addr_1 & RAMBMSK)) nByte = RAMBLK + (addr_1 & RAMBMSK) - addr_1;
            qspi_qread (addr_1, nByte, pbyt);
            qspi_wait ();
            addr_1 += nByte;
            pbyt += nByte;
            }
        nPix -= nCol;
        nColCur += nCol;
        if (nColCur == nColEnd)
            {
            nColCur = nColBeg;
            ++nRowCur;
            if (nRowCur == nRowEnd) nRowCur = nRowBeg;
            }
        }
    }

static colour_t load_colour (void)
    {
    colour_t clr;
    load_pixels (&clr, 1);
    return clr;
    }

#if REF_MODE == 1
void refresh_now (void)
    {
    if (bBuffer) load_lcd ();
    }

void refresh_on (void)
    {
    if (bBuffer) load_lcd ();
    bBuffer = false;
    reflag = 2;
    }

void refresh_rst (void)
    {
    bBuffer = false;
    reflag = 2;
    }

void refresh_off (void)
    {
    reflag = 1;
    bBuffer = true;
    if (! bBuffer) save_lcd ();
    }

#elif REF_MODE == 3
#define LIMIT   0x4B000
// #define LIMIT   0x1000

void refresh_now (void)
    {
    // printf ("refresh_now...\n");
    if ( rfm == rfmBuffer )
        {
        if (bBuffer) load_lcd ();
        }
    else if ( rfm == rfmQueue )
        {
        vduflush ();
        }
    // printf ("...done\n");
    }

void refresh_on (void)
    {
    // printf ("refresh_on...\n");
    if ( rfm == rfmBuffer )
        {
        if (bBuffer) load_lcd ();
        bBuffer = false;
        }
    else if ( rfm == rfmQueue )
        {
        vduqterm ();
        }
    reflag = 2;
    // printf ("...done\n");
    }

void refresh_rst (void)
    {
    // printf ("refresh_rst...\n");
    if ( rfm == rfmBuffer )
        {
        bBuffer = false;
        }
    reflag = 2;
    // printf ("...done\n");
    }

void refresh_off (void)
    {
    // printf ("refresh_off...\n");
    reflag = 1;
    if ( rfm == rfmBuffer )
        {
        if (! bBuffer)
            {
            bBuffer = true;
            save_lcd ();
            }
        }
    else if ( rfm == rfmQueue )
        {
        vduqinit ();
        }
    // printf ("...done\n");
    }

const char *checkbuf (void)
    {
    return NULL;
    }
#endif
