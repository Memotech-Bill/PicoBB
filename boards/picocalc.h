/*
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_PICOCALC_H
#define _BOARDS_PICOCALC_H

// --- UART ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

// --- MPU UART ---
#ifndef PICO_MPU_UART
#define PICO_MPU_UART 1
#endif
#ifndef PICO_MPU_UART_TX_PIN
#define PICO_MPU_UART_TX_PIN 8
#endif
#ifndef PICO_MPU_UART_RX_PIN
#define PICO_MPU_UART_RX_PIN 9
#endif

// --- I2C ---
#ifndef PICO_KEYBOARD_I2C
#define PICO_KEYBOARD_I2C 1
#endif
#ifndef PICO_KEYBOARD_SDA_PIN
#define PICO_KEYBOARD_SDA_PIN 6
#endif
#ifndef PICO_KEYBOARD_SCL_PIN
#define PICO_KEYBOARD_SCL_PIN 7
#endif
#ifndef PICO_KEYBOARD_SPEED
#define PICO_KEYBOARD_SPEED 10000
#endif
#ifndef PICO_KEYBOARD_ADDR
#define PICO_KEYBOARD_ADDR  0x1F
#endif

// --- SDCard SPI ---
#ifndef PICO_SD_SPI
#define PICO_SD_SPI 0
#endif
#ifndef PICO_SD_DAT0_PIN
#define PICO_SD_DAT0_PIN 16
#endif
#ifndef PICO_SD_CS_PIN
#define PICO_SD_CS_PIN 17
#endif
#ifndef PICO_SD_CLK_PIN
#define PICO_SD_CLK_PIN 18
#endif
#ifndef PICO_SD_CMD_PIN
#define PICO_SD_CMD_PIN 19
#endif
#ifndef PICO_SD_DETECT_PIN
#define PICO_SD_DETECT_PIN  22
#endif

// --- LCD SPI ---
#ifndef PICO_LCD_SPI
#define PICO_LCD_SPI 1
#endif
#ifndef PICO_LCD_SPEED
#define PICO_LCD_SPEED  25000000
#endif
#ifndef PICO_LCD_CLK_PIN
#define PICO_LCD_CLK_PIN    10
#endif
#ifndef PICO_LCD_TX_PIN
#define PICO_LCD_TX_PIN     11
#endif
#ifndef PICO_LCD_RX_PIN
#define PICO_LCD_RX_PIN     12
#endif
#ifndef PICO_LCD_CS_PIN
#define PICO_LCD_CS_PIN     13
#endif
#ifndef PICO_LCD_DC_PIN
#define PICO_LCD_DC_PIN     14
#endif
#ifndef PICO_LCD_RST_PIN
#define PICO_LCD_RST_PIN    15
#endif
#ifndef PICO_LCD_ROWS
#define PICO_LCD_ROWS       320
#endif
#ifndef PICO_LCD_COLUMNS
#define PICO_LCD_COLUMNS    320
#endif

// --- PSRAM ---
#ifndef PICO_PSRAM_SIO0_PIN
#define PICO_PSRAM_SIO0_PIN 2
#endif
#ifndef PICO_PSRAM_SIO1_PIN
#define PICO_PSRAM_SIO1 3
#endif
#ifndef PICO_PSRAM_SIO2_PIN
#define PICO_PSRAM_SIO2_PIN 4
#endif
#ifndef PICO_PSRAM_SIO3_PIN
#define PICO_PSRAM_SIO3_PIN 5
#endif
#ifndef PICO_PSRAM_CST_PIN
#define PICO_PSRAM_CST_PIN   20
#endif
#ifndef PICO_PSRAM_CLK_PIN
#define PICO_PSRAM_CLK_PIN 21
#endif

// --- SOUND ---

#ifndef PICO_AUDIO_PWM_L_PIN
#define PICO_AUDIO_PWM_L_PIN    26
#endif
#ifndef PICO_AUDIO_PWM_R_PIN
#define PICO_AUDIO_PWM_R_PIN    27
#endif

#endif
