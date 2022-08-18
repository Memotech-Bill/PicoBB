#  This file is for using the SDK to create software
#  that runs on the Raspberry Pi Pico.  Please set
#
#     PICO_SDK_PATH
#
#  to the subdirectory where you have installed the SDK. */

cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
if ( ${STDIO} STREQUAL "PICO" )
  include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../boards)

project(bbcbasic C CXX ASM)
pico_sdk_init()

if ( NOT STDIO )
  set(STDIO "USB+UART")
endif()

if ( NOT BBC_SRC )
  set(BBC_SRC "../../BBCSDL")
endif()
message(STATUS "BBC BASIC Upstream source from ${BBC_SRC}")

if (TARGET tinyusb_device)
  add_executable(bbcbasic 
    ${BBC_SRC}/src/bbdata_arm_32.s
    ${BBC_SRC}/src/bbmain.c
    ../../src/bbccos.c
    ../../src/bbpico.c
    ../../src/bbasmb_arm_v6m.c 
    ../../src/lfswrap.c 
    ../../src/fault.c
    ../../src/pico/sympico.c
    ../../src/pico/${SYM_NAME}_stub.c
    )
  if (${MIN_STACK})
    message(STATUS "Expression evaluation revised to reduce stack usage")
    target_sources(bbcbasic PRIVATE
      ../../src/bbexec2.c
      ../../src/bbeval2.c
      )
  else()
    message(STATUS "Using upstream expression evaluation code")
    target_sources(bbcbasic PRIVATE
      ${BBC_SRC}/src/bbexec.c
      ${BBC_SRC}/src/bbeval.c
      )
  endif()
  if (${STACK_CHECK} GREATER_EQUAL 4)
    message(STATUS "Using memory protection to detect stack overrun")
    target_sources(bbcbasic PRIVATE ../../src/pico/stack_trap.c)
  else()
    target_sources(bbcbasic PRIVATE ../../m0FaultDispatch/m0FaultDispatch.c)
  endif()
      
  pico_set_printf_implementation(bbcbasic compiler)
  target_compile_options(bbcbasic PRIVATE -O1 -g)
  target_link_options(bbcbasic PRIVATE -g)
  target_link_libraries(bbcbasic
    hardware_adc
    hardware_claim
    hardware_clocks
    hardware_divider
    hardware_dma
    hardware_exception
    hardware_flash
    hardware_gpio
    hardware_i2c
    hardware_interp
    hardware_irq
    hardware_pio
    hardware_pll
    hardware_pwm
    hardware_rtc
    hardware_spi
    hardware_sync
    hardware_timer
    hardware_uart
    hardware_vreg
    hardware_watchdog
    hardware_xosc
    pico_bit_ops
    pico_divider
    pico_double
    pico_float
    pico_multicore
    pico_printf
    pico_printf_headers
    pico_stdio
    pico_stdio_uart
    pico_stdlib
    pico_sync
    pico_time
    pico_unique_id
    pico_util
    )
  target_compile_definitions(bbcbasic PUBLIC
    PICO
    PICO_STACK_CHECK=${STACK_CHECK}
    )
  target_include_directories(bbcbasic PRIVATE
    ../../include
    ${BBC_SRC}/include
    ../../m0FaultDispatch
    )

  if( ${CYW43} STREQUAL "GPIO" )
    message(STATUS "Pico W GPIO (LED) 0nly support")
    target_link_libraries(bbcbasic
      pico_cyw43_arch_none
      )
    target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=none)
  elseif( ${CYW43} STREQUAL "POLL" )
    message(STATUS "Pico W CYW43 poll support")
    target_link_libraries(bbcbasic
      pico_cyw43_arch_lwip_poll
      )
    target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=poll)
  elseif( ${CYW43} )
    message(STATUS "Pico W CYW43 background support")
    target_link_libraries(bbcbasic
      pico_cyw43_arch_lwip_threadsafe_background
      )
    target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=background)
  endif()

  if ( ${SOUND} STREQUAL "I2S")
    message(STATUS "Generate sound using I2S output")
    target_compile_definitions(bbcbasic PUBLIC -DPICO_SOUND=1)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/../../src/pico/sound.pio)
    target_sources(bbcbasic PRIVATE
      ../../src/sn76489.c
      ../../src/pico/pico_snd.c
      )
  elseif ( ${SOUND} STREQUAL "PWM")
    message(STATUS "Generate sound using PWM")
    target_compile_definitions(bbcbasic PUBLIC -DPICO_SOUND=2)
    target_sources(bbcbasic PRIVATE
      ../../src/sn76489.c
      ../../src/pico/pico_snd.c
      )
    target_link_libraries(bbcbasic hardware_pwm)
  elseif ( ${SOUND} STREQUAL "SDL")
    message(STATUS "Generate high quality sound using PWM on core 1")
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_SOUND=3
      -DPICO_MCLOCK
      )
    target_sources(bbcbasic PRIVATE
      ${BBC_SRC}/src/sound_sdl.c
      )
    target_link_libraries(bbcbasic hardware_pwm)
  endif()

  if ( ${LFS} )
    message(STATUS "Including LFS filesystem for flash storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_LFS)
    target_sources(bbcbasic PRIVATE
      ../../src/lfsmcu.c
      ../../littlefs/lfs.c
      ../../littlefs/lfs_util.c
      )
    target_include_directories(bbcbasic PRIVATE
      ../../littlefs
      )
  endif()

  if ( ${FAT} )
    message(STATUS "Including FAT filesystem for SD card storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_FAT)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/../../src/pico/sd_spi.pio)
    target_sources(bbcbasic PRIVATE
      ../../fatfs/ff.c
      ../../fatfs/ffsystem.c
      ../../fatfs/ffunicode.c
      ../../src/pico/ff_disk.c
      ../../src/pico/sd_spi2.c
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      )
    target_link_libraries(bbcbasic hardware_dma hardware_pio)
  endif()

  if ( ${SERIAL_DEV} )
    target_compile_definitions(bbcbasic PRIVATE -DSERIAL_DEV=${SERIAL_DEV})
    if ( ${SERIAL_DEV} GREATER 0 )
      target_sources(bbcbasic PRIVATE ../../src/pico/picoser.c)
    endif()
  endif()
  
  if ( ${STDIO} STREQUAL "PICO" )
    message(STATUS "BBC Basic console I/O will be on USB keyboard and VGA screen")
    pico_enable_stdio_usb(bbcbasic 0)
    if (${PRINTER})
      pico_enable_stdio_uart(bbcbasic 1)
      target_compile_definitions(bbcbasic PRIVATE -DHAVE_PRINTER)
    elseif (${SERIAL_DEV} EQUAL -1)
      pico_enable_stdio_uart(bbcbasic 1)
    else()
      pico_enable_stdio_uart(bbcbasic 0)
    endif()
    target_sources(bbcbasic PRIVATE
      ../../src/fbufvdu.c
      ../../src/pico_gui/picokbd.c
      ../../src/pico_gui/picofbuf.c
      ../../src/pico_gui/framebuffer.S
      )
    target_compile_definitions(bbcbasic PRIVATE -DPICO_GUI)
    target_compile_definitions(bbcbasic PRIVATE -DGUI_SERIAL=${GUI_SERIAL})
    
    target_compile_definitions(bbcbasic PUBLIC -DPICO_PANIC_FUNCTION=message)
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=402
      -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      ../../include/pico_gui
      )
    target_link_libraries(bbcbasic
      pico_scanvideo_dpi
      hardware_interp
      tinyusb_host
      tinyusb_board
      )
  elseif ( ${STDIO} STREQUAL "USB+UART" )
    message(STATUS "BBC Basic console I/O will be on USB or UART")
    target_link_libraries(bbcbasic
      pico_stdio_usb
      )
    pico_enable_stdio_usb(bbcbasic 1)
    pico_enable_stdio_uart(bbcbasic 1)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB -DSTDIO_UART)
    target_compile_definitions(bbcbasic PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)
  elseif ( ${STDIO} STREQUAL "UART" )
    message(STATUS "BBC Basic console I/O will be on UART")
    pico_enable_stdio_usb(bbcbasic 0)
    pico_enable_stdio_uart(bbcbasic 1)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_UART)
  else()
    message(STATUS "BBC Basic console I/O will be on USB")
    target_link_libraries(bbcbasic
      pico_stdio_usb
      )
    pico_enable_stdio_usb(bbcbasic 1)
    pico_enable_stdio_uart(bbcbasic 0)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB)
    target_compile_definitions(bbcbasic PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)
  endif()
  
  if ( ${STDIO} STREQUAL "PICO" )
    if ( ${SOUND} STREQUAL "SDL")
      message(FATAL_ERROR "Can not use core 1 for both sound and video")
    endif()
    target_compile_definitions(bbcbasic PUBLIC
      PICO_STACK_SIZE=0x2B800
      PICO_CORE1_STACK_SIZE=0x800
      )
    target_link_libraries(bbcbasic pico_multicore)
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/../pico_gui/gui_pico.ld)
  else()
    target_compile_definitions(bbcbasic PUBLIC
      PICO_STACK_SIZE=0x3B800
      PICO_CORE1_STACK_SIZE=0x800
      )
    target_link_libraries(bbcbasic pico_multicore)
    if ( ${CYW43} )
      pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico_net_mc.ld)
    else()
      pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico_mc.ld)
    endif()
  endif()
  
  pico_add_extra_outputs(bbcbasic)
elseif(PICO_ON_DEVICE)
  message(WARNING "Not building bbcbasic because TinyUSB submodule is not initialized in the SDK")
endif()
