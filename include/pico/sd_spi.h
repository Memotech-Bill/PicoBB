/* sp_sdi.h - SPI routines to access SD card */

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>

#ifndef SD_CLK_PIN
#ifdef PICO_SD_CLK_PIN
#define SD_CLK_PIN      PICO_SD_CLK_PIN
#else
#error SD Card CLK pin not defined. Specify a board including SD card.
#endif
#endif

#ifndef SD_MOSI_PIN
#ifdef PICO_SD_CMD_PIN
#define SD_MOSI_PIN     PICO_SD_CMD_PIN
#else
#error SD Card CMD / MOSI pin not defined. Specify a board including SD card.
#endif
#endif

#ifndef SD_MISO_PIN
#ifdef PICO_SD_DAT0_PIN
#define SD_MISO_PIN     PICO_SD_DAT0_PIN
#else
#error SD Card DAT0 / MISO pin not defined. Specify a board including SD card.
#endif
#endif

#ifndef SD_CS_PIN
#ifdef PICO_SD_DAT0_PIN
#define SD_CS_PIN       ( PICO_SD_DAT0_PIN + 3 * PICO_SD_DAT_PIN_INCREMENT )
#else
#error SD Card DAT3 / CS pin not defined. Specify a board including SD card.
#endif
#endif

typedef enum {sdtpUnk, sdtpVer1, sdtpVer2, sdtpHigh} SD_TYPE;
extern SD_TYPE sd_type;

bool sd_spi_init (void);
void sd_spi_term (void);
bool sd_spi_read (uint lba, uint8_t *buff);
bool sd_spi_write (uint lba, const uint8_t *buff);

#endif
