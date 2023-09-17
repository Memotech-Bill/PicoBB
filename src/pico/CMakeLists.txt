#  This file is for using the SDK to create software
#  that runs on the Raspberry Pi Pico.  Please set
#
#     PICO_SDK_PATH
#
#  to the subdirectory where you have installed the SDK. */

cmake_minimum_required(VERSION 3.12)

if( DEFINED CYW43 )
  set(PICO_CYW43_SUPPORTED 1)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
if (( "${STDIO}" STREQUAL "PICO" ) OR ( ${GRAPH} ))
  message(STATUS "Include PICO-EXTRAS")
  include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../boards)

project(bbcbasic C CXX ASM)
pico_sdk_init()

if ( NOT DEFINED STDIO )
  set(STDIO "USB+UART")
endif()

string(TOUPPER ${STDIO} STDIO)
string(REPLACE "+" ";" STDIO_LIST ${STDIO})

if ( NOT DEFINED BBC_SRC )
  set(BBC_SRC "../../BBCSDL")
endif()
message(STATUS "BBC BASIC Upstream source from ${BBC_SRC}")

if (TARGET tinyusb_device)
  add_executable(bbcbasic 
    ${BBC_SRC}/src/bbdata_arm_32.s
    ${BBC_SRC}/src/bbmain.c
    ../../src/bbccos.c
    ../../src/bbpico.c
    ../../src/picocli.c
    ../../src/bbasmb_arm_v6m.c 
    ../../src/lfswrap.c 
    ../../src/fault.c
    ../../src/pico/sympico.c
    ../../src/pico/crctab.c
    ${CMAKE_BINARY_DIR}/pico_stub.c
    )
  if (${MIN_STACK} STREQUAL "X")
    message(STATUS "Expression evaluation revised to reduce stack usage")
    target_compile_definitions(bbcbasic PUBLIC -DMIN_STACK=2)
    target_sources(bbcbasic PRIVATE
      ../../src/bbexec2.c
      ../../src/bbeval2.c
      )
  else()
    if (${MIN_STACK})
      message(STATUS "Using upstream expression evaluation code with REDUCE_STACK_SIZE")
      target_compile_definitions(bbcbasic PUBLIC
        -DREDUCE_STACK_SIZE
        -DMIN_STACK=1
        )
    else()
      message(STATUS "Using upstream expression evaluation code")
      target_compile_definitions(bbcbasic PUBLIC -DMIN_STACK=0)
    endif()
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
    CAN_SET_RTC
    PICO_STACK_CHECK=${STACK_CHECK}
    PICO_PANIC_FUNCTION=sys_panic
    )
  target_include_directories(bbcbasic PRIVATE
    ../../include
    ${BBC_SRC}/include
    ../../m0FaultDispatch
    ${CMAKE_BINARY_DIR}
    )

  if( DEFINED CYW43 )
    if( "${CYW43}" STREQUAL "GPIO" )
      message(STATUS "Pico W GPIO (LED) only support")
      target_link_libraries(bbcbasic
        pico_cyw43_arch_none
        )
      target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=1)
      set(CYW43_LINK 0)
    elseif( "${CYW43}" STREQUAL "POLL" )
      message(STATUS "Pico W CYW43 poll support")
      target_sources(bbcbasic PRIVATE ../../src/pico/network_cyw43.c)
      target_link_libraries(bbcbasic
        pico_cyw43_arch_lwip_poll
        pico_lwip
        pico_lwip_nosys)
      target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=2)
      set(CYW43_LINK 1)
    elseif( CYW43 )
      message(STATUS "Pico W CYW43 background support")
      target_sources(bbcbasic PRIVATE ../../src/pico/network_cyw43.c)
      target_link_libraries(bbcbasic
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip
        pico_lwip_nosys)
      target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=3)
      set(CYW43_LINK 1)
    endif()
  else()
    set(CYW43_LINK 0)
    target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=0)
  endif()

  if ( "${SOUND}" STREQUAL "I2S")
    message(STATUS "Generate sound using I2S output")
    target_compile_definitions(bbcbasic PUBLIC -DPICO_SOUND=1)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/../../src/pico/sound.pio)
    target_sources(bbcbasic PRIVATE
      ../../src/sn76489.c
      ../../src/pico/pico_snd.c
      )
  elseif ( "${SOUND}" STREQUAL "PWM")
    message(STATUS "Generate sound using PWM")
    target_compile_definitions(bbcbasic PUBLIC -DPICO_SOUND=2)
    target_sources(bbcbasic PRIVATE
      ../../src/sn76489.c
      ../../src/pico/pico_snd.c
      )
    target_link_libraries(bbcbasic hardware_pwm)
  elseif ( "${SOUND}" STREQUAL "SDL")
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
    target_compile_definitions(bbcbasic PUBLIC
      -DHAVE_LFS
      -DROOT_SIZE=0x100000
      -DROOT_OFFSET=0x100000
      -DPICO_FLASH_BANK_TOTAL_SIZE=0x2000           # = 2 * FLASH_SECTOR_SIZE
      -DPICO_FLASH_BANK_STORAGE_OFFSET=0x0FE000     # = ROOT_OFFSET - PICO_FLASH_BANK_TOTAL_SIZE
      )
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
  
  if ( "${STDIO}" STREQUAL "PICO" )
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
  else()
    target_sources(bbcbasic PRIVATE
      ../../src/pico/zmodem.c
      )
    if ( "USB" IN_LIST STDIO_LIST )
      message(STATUS "BBC Basic console I/O will be on USB")
      target_sources(bbcbasic PRIVATE
        ../../src/pico/pico_boot.c
        )
      target_link_libraries(bbcbasic
        pico_stdio_usb
        )
      pico_enable_stdio_usb(bbcbasic 1)
      target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB -DSTDIO_UART)
      target_compile_definitions(bbcbasic PUBLIC
        PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
        PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
        )
    else()
      pico_enable_stdio_usb(bbcbasic 0)
    endif()
    if ( "UART" IN_LIST STDIO_LIST )
      message(STATUS "BBC Basic console I/O will be on UART")
      target_link_libraries(bbcbasic
        pico_stdio_uart
        )
      pico_enable_stdio_uart(bbcbasic 1)
      target_compile_definitions(bbcbasic PRIVATE -DSTDIO_UART)
    else()
      pico_enable_stdio_uart(bbcbasic 0)
    endif()
    if ( "BT" IN_LIST STDIO_LIST )
      message(STATUS "BBC Basic console I/O will be on Bluetooth")
      target_sources(bbcbasic PRIVATE
        stdio_bt.c
        )
      target_link_libraries(bbcbasic
        pico_btstack_classic
        pico_btstack_ble
        pico_btstack_cyw43
        )
      target_compile_definitions(bbcbasic PRIVATE -DSTDIO_BT)
    endif()
  endif()

  if( (${GRAPH}) AND (NOT ("${STDIO}" STREQUAL "PICO")))
    message(STATUS "Framebuffer Graphics Support")
    target_sources(bbcbasic PRIVATE
      ../../src/fbufvdu.c
      ../../src/pico_gui/picofbuf.c
      ../../src/pico_gui/framebuffer.S
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      )
    target_compile_definitions(bbcbasic PUBLIC -DPICO_GRAPH)
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=402
      -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
      )
    target_link_libraries(bbcbasic
      pico_scanvideo_dpi
      hardware_interp
      )
  endif()
  
  if (( "${STDIO}" STREQUAL "PICO" ) OR ( ${GRAPH} ))
    if ( "${SOUND}" STREQUAL "SDL")
      message(FATAL_ERROR "Can not use core 1 for both sound and video")
    endif()
  endif()

  if (NOT ("${OS_RAM}" STREQUAL "" ))
    message(STATUS "Link with custom ${OS_RAM}kB of OS RAM storage")
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/picobb.ld
      COMMAND ${CMAKE_SOURCE_DIR}/memmap.py ${CMAKE_BINARY_DIR}/picobb.ld ${CMAKE_SOURCE_DIR}/bbc_pico_mc.ld $(OS_RAM)
      )
    add_custom_target(link_file ALL DEPENDS ${CMAKE_BINARY_DIR}/picobb.ld)
    add_dependencies(bbcbasic link_file)
    pico_set_linker_script(bbcbasic ${CMAKE_BINARY_DIR}/picobb.ld)
    string(CONCAT stk_expr "1024*(262-" ${OS_RAM} ")")
    math(EXPR stk_size "${stk_expr}" OUTPUT_FORMAT HEXADECIMAL)
    target_compile_definitions(bbcbasic PUBLIC
      PICO_STACK_SIZE=${stk_size}
      PICO_CORE1_STACK_SIZE=0x800
      )
  elseif (( "${STDIO}" STREQUAL "PICO" ) OR ( ${GRAPH} ))
    if( ${CYW43_LINK} )
      message(STATUS "Link with space for VGA video and CYW43 driver")
      target_compile_definitions(bbcbasic PUBLIC
        PICO_STACK_SIZE=0x21800
        PICO_CORE1_STACK_SIZE=0x800
        )
      pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/../pico_gui/gui_pico_net.ld)
    else()
      message(STATUS "Link with space for VGA video")
      target_compile_definitions(bbcbasic PUBLIC
        PICO_STACK_SIZE=0x2B800
        PICO_CORE1_STACK_SIZE=0x800
        )
      pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/../pico_gui/gui_pico.ld)
    endif()
  else()
    if( ${CYW43_LINK} )
      message(STATUS "Link with space for CYW43 driver")
      target_compile_definitions(bbcbasic PUBLIC
        PICO_STACK_SIZE=0x2F800
        PICO_CORE1_STACK_SIZE=0x800
        )
      pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico_net_mc.ld)
    else()
      message(STATUS "Link with 2KB multicore stack")
      target_compile_definitions(bbcbasic PUBLIC
        PICO_STACK_SIZE=0x3B800
        PICO_CORE1_STACK_SIZE=0x800
        )
      pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico_mc.ld)
    endif()
  endif()
  
  pico_add_extra_outputs(bbcbasic)
elseif(PICO_ON_DEVICE)
  message(WARNING "Not building bbcbasic because TinyUSB submodule is not initialized in the SDK")
endif()
