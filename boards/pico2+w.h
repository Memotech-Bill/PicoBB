// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// This header may be included by other board headers as "boards/pico2+w.h"
#ifndef _BOARDS_PICO2_PW_H
#define _BOARDS_PICO2_PW_H

// For board detection
#define RASPBERRYPI_PICO2_PW

// --- LED ---
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif
// no PICO_DEFAULT_WS2812_PIN

#include <boards/pico2_w.h>

#endif
