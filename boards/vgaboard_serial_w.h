/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Revised version for when the links between GPIOs 20 & 21 and SD Data 1 & 2
// are intact. This board definition is used if it is intended to use the
// serial port, in which case the SD card cannot be used.
// Memotech-Bill, March 2022.

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_VGABOARD_H
#define _BOARDS_VGABOARD_H

// For board detection
#define RASPBERRYPI_VGABOARD_SERIAL

// Audio pins. I2S BCK, LRCK are on the same pins as PWM L/R.
// - When outputting I2S, PWM sees BCK and LRCK, which should sound silent as
//   they are constant duty cycle, and above the filter cutoff
// - When outputting PWM, I2S DIN should be low, so I2S should remain silent.
#define VGABOARD_I2S_DIN_PIN 26
#define VGABOARD_I2S_BCK_PIN 27
#define VGABOARD_I2S_LRCK_PIN 28

#define VGABOARD_PWM_L_PIN 28
#define VGABOARD_PWM_R_PIN 27

#define VGABOARD_VGA_COLOR_PIN_BASE 0
#define VGABOARD_VGA_SYNC_PIN_BASE 16

// Note DAT1/2 are shared with UART TX/RX (pull jumpers off header to access
// UART pins and disconnect SD DAT1/2)
#define VGABOARD_SD_CLK_PIN 5
#define VGABOARD_SD_CMD_PIN 18
#define VGABOARD_SD_DAT0_PIN 19

// Note buttons are shared with VGA colour LSBs -- if using VGA, you can float
// the pin on VSYNC assertion and sample on VSYNC deassertion
#define VGABOARD_BUTTON_A_PIN 0
#define VGABOARD_BUTTON_B_PIN 6
#define VGABOARD_BUTTON_C_PIN 11

#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 1
#endif

#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 20
#endif

#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 21
#endif

#define PICO_SCANVIDEO_COLOR_PIN_BASE VGABOARD_VGA_COLOR_PIN_BASE
#define PICO_SCANVIDEO_SYNC_PIN_BASE VGABOARD_VGA_SYNC_PIN_BASE

#define PICO_AUDIO_I2S_DATA_PIN VGABOARD_I2S_DIN_PIN
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE VGABOARD_I2S_BCK_PIN

#define PICO_AUDIO_PWM_L_PIN VGABOARD_PWM_L_PIN
#define PICO_AUDIO_PWM_R_PIN VGABOARD_PWM_R_PIN

#define PICO_VGA_BOARD_SERIAL

// vgaboard has a Pico on it, so default anything we haven't set above

// CMake does not read these from included header files

// pico_cmake_set PICO_PLATFORM        = rp2040
// pico_cmake_set PICO_CYW43_SUPPORTED = 1
// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (2 * 1024 * 1024)

#include "pico+w.h"

#endif
