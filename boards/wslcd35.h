// Addon board definition for Waveshare Pico-ResTouch-LCD-3.5


#ifndef _BOARDS_WAVESHARE_LCD_3_5_H
#define _BOARDS_WAVESHARE_LCD_3_5_H

// For board detection
#define WAVESHARE_LCD_3_5

// --- LCD ---
#ifndef LCD_SPI_PORT
#define LCD_SPI_PORT    spi1
#endif
#ifndef LCD_DC_PIN
#define LCD_DC_PIN      8
#endif
#ifndef LCD_CS_PIN
#define LCD_CS_PIN      9
#endif
#ifndef LCD_CLK_PIN
#define LCD_CLK_PIN     10
#endif
#ifndef LCD_MOSI_PIN
#define LCD_MOSI_PIN    11
#endif
#ifndef LCD_MISO_PIN
#define LCD_MISO_PIN	12
#endif
#ifndef LCD_BKL_PIN
#define LCD_BKL_PIN     13
#endif
#ifndef LCD_RST_PIN
#define LCD_RST_PIN     15
#endif

// --- Touch Pad ---
#ifndef TP_SPI_PORT
#define TP_SPI_PORT     spi1
#endif
#ifndef TP_CLK_PIN
#define TP_CLK_PIN	    10
#endif
#ifndef TP_MOSI_PIN
#define TP_MOSI_PIN	    11
#endif
#ifndef TP_MISO_PIN
#define TP_MISO_PIN	    12
#endif
#ifndef TP_CS_PIN
#define TP_CS_PIN	    16
#endif
#ifndef TP_IRQ_PIN
#define TP_IRQ_PIN	    17
#endif

// --- SD Card ---
#ifndef SD_SPI_PORT
#define SD_SPI_PORT     spi1
#endif
#ifndef SD_CLK_PIN
#define SD_CLK_PIN	    10
#endif
#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN	    11
#endif
#ifndef SD_MISO_PIN
#define SD_MISO_PIN	    12
#endif
#ifndef SD_CS_PIN
#define SD_CS_PIN	    22
#endif

#endif
