/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_
#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#if (TUSB_VERSION_MAJOR == 0) && (TUSB_VERSION_MINOR >= 17)
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

// Enable Host stack
#define CFG_TUH_ENABLED       1

#if CFG_TUSB_MCU == OPT_MCU_RP2040
  // #define CFG_TUH_RPI_PIO_USB   1 // use pio-usb as host controller
  // #define CFG_TUH_MAX3421       1 // use max3421 as host controller

  // host roothub port is 1 if using either pio-usb or max3421
  #if (defined(CFG_TUH_RPI_PIO_USB) && CFG_TUH_RPI_PIO_USB) || (defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421)
    #define BOARD_TUH_RHPORT      1
  #endif
#endif

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUH_MAX_SPEED     BOARD_TUH_MAX_SPEED

//------------------------- Board Specific --------------------------

// RHPort number used for host can be defined by board.mk, default to port 0
#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT      0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif
#endif

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#if CFG_TUSB_MCU == OPT_MCU_LPC43XX || CFG_TUSB_MCU == OPT_MCU_LPC18XX || CFG_TUSB_MCU == OPT_MCU_MIMXRT10XX
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_HOST | OPT_MODE_HIGH_SPEED)
#else
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUH_HUB                 1
#define CFG_TUH_HID_KEYBOARD        1
#define CFG_TUH_HID_MOUSE           0
#define CFG_TUSB_HOST_HID_GENERIC   0 // (not yet supported)
#define CFG_TUH_MSC                 0
#define CFG_TUH_CDC                 0

#define CFG_TUSB_HOST_DEVICE_MAX    (CFG_TUH_HUB ? 5 : 1) // normal hub has 4 ports

#if (TUSB_VERSION_MAJOR == 0) && (TUSB_VERSION_MINOR >= 17)
// Size of buffer to hold descriptors and other data used for enumeration
#define CFG_TUH_ENUMERATION_BUFSIZE 256

#define CFG_TUH_HUB                 1 // number of supported hubs
#define CFG_TUH_CDC_FTDI            0 // FTDI Serial.  FTDI is not part of CDC class, only to re-use CDC driver API
#define CFG_TUH_CDC_CP210X          0 // CP210x Serial. CP210X is not part of CDC class, only to re-use CDC driver API
#define CFG_TUH_CDC_CH34X           0 // CH340 or CH341 Serial. CH34X is not part of CDC class, only to re-use CDC driver API
#define CFG_TUH_HID                 (3*CFG_TUH_DEVICE_MAX) // typical keyboard + mouse device can have 3-4 HID interfaces
#define CFG_TUH_VENDOR              0

// max device support (excluding hub device): 1 hub typically has 4 ports
#define CFG_TUH_DEVICE_MAX          (3*CFG_TUH_HUB + 1)

//------------- HID -------------//
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64

#elif ( PICO_SDK_VERSION_MAJOR == 1 ) && ( PICO_SDK_VERSION_MINOR >= 2 )
// Additional definitions required by tinyusb 0.10 in sdk 1.2
#define CFG_TUSB_HOST_DEVICE_MAX    (CFG_TUH_HUB ? 5 : 1) // normal hub has 4 ports

// Size of buffer to hold descriptors and other data used for enumeration
#define CFG_TUH_ENUMERATION_BUFSZIE 256

#define CFG_TUH_HID                 ( CFG_TUH_HID_KEYBOARD + CFG_TUH_HID_MOUSE )
#define CFG_TUH_VENDOR              0

//------------- HID -------------//

#define CFG_TUH_HID_EP_BUFSIZE      64
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
