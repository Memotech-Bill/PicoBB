// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// This header may be included by other board headers as "boards/pico+w.h"
#ifndef _BOARDS_PICO_PW_H
#define _BOARDS_PICO_PW_H

// For board detection
#define RASPBERRYPI_PICO_PW

// CMake does not read these from included header files

// pico_cmake_set PICO_PLATFORM        = rp2040
// pico_cmake_set PICO_CYW43_SUPPORTED = 1
// pico_cmake_set_default PICO_FLASH_SIZE_BYTES = (2 * 1024 * 1024)

// --- LED ---
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif
// no PICO_DEFAULT_WS2812_PIN

#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 1
#endif

#ifndef PICO_RP2040_B1_SUPPORTED
#define PICO_RP2040_B1_SUPPORTED 1
#endif

#include <boards/pico_w.h>

#endif
