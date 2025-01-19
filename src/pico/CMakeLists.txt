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

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../boards)
if(DEFINED ENV{PICO_SDK_PATH})
  include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
else()
  message(FATAL_ERROR "Please set environment variable PICO_SDK_PATH to location of pico-sdk")
endif()

if(NOT DEFINED STDIO)
  set(STDIO "USB+UART")
endif()
string (TOUPPER "${STDIO}" STDIO)
string (TOUPPER "${GRAPH}" GRAPH)
if ( "${STDIO}" STREQUAL "PICO" )
  if (NOT GRAPH)
    set (GRAPH "VGA")
  endif ()
endif ()
if ( "${GRAPH}" STREQUAL "Y" )
  set (GRAPH "VGA")
endif ()
if ( "${GRAPH}" STREQUAL "VGA" )
  if(DEFINED ENV{PICO_EXTRAS_PATH})
    message(STATUS "Include PICO-EXTRAS")
    include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
  else()
    message(FATAL_ERROR "Please set environment variable PICO_EXTRAS_PATH to location of pico-extras")
  endif()
endif()

project(bbcbasic C CXX ASM)
pico_sdk_init()

# Work around for SDK v1.x
if(NOT DEFINED PICO_CHIP)
  message(STATUS "PICO_CHIP not defined, assumming rp2040")
  set(PICO_CHIP "rp2040")
else()
  message(STATUS "PICO_CHIP set to ${PICO_CHIP}")
endif()

if(NOT OPTIMISE)
  set(OPTIMISE "-O1")
  # if((${CMAKE_C_COMPILER_VERSION} VERSION_GREATER_EQUAL "12.2.1") AND (${PICO_CHIP} STREQUAL "rp2350") AND (NOT GRAPH))
  #   set(OPTIMISE "-Oz")
  # endif()
endif()
message(STATUS "Compiling using optimisation level ${OPTIMISE}")

string(REPLACE "+" ";" STDIO_LIST ${STDIO})

if ( NOT DEFINED BBC_SRC )
  set(BBC_SRC "../../BBCSDL")
endif()
message(STATUS "BBC BASIC Upstream source from ${BBC_SRC}")

if (TARGET tinyusb_device)
  add_executable(bbcbasic)

  add_subdirectory(symbols)
  set_source_files_properties(${CMAKE_BINARY_DIR}/pico_stub.c PROPERTIES GENERATED TRUE)
  set_source_files_properties(${CMAKE_BINARY_DIR}/sympico.h PROPERTIES GENERATED TRUE)
  add_dependencies(bbcbasic
    stub_file
    sym_file
    )

  target_sources(bbcbasic PRIVATE
    ${BBC_SRC}/src/bbdata_arm_32.s
    ${BBC_SRC}/src/bbmain.c
    ${BBC_SRC}/src/bbccos.c
    ../../src/bbpico.c
    ../../src/picocli.c
    ../../src/lfswrap.c 
    ../../src/fault.c
    ../../src/pico/sympico.c
    ../../src/pico/crctab.c
    ${CMAKE_BINARY_DIR}/pico_stub.c
    )
  if (${PICO_CHIP} STREQUAL "rp2350")
    target_sources(bbcbasic PRIVATE
      ../../src/bbasmb_arm_v8m.c
      )
    set_source_files_properties(${BBC_SRC}/src/bbmain.c
        PROPERTIES COMPILE_DEFINITIONS xeq=pico_xeq)
  else()
    target_sources(bbcbasic PRIVATE
      ../../src/bbasmb_arm_v6m.c
      )
  endif()
  if (ADDON)
    string (TOLOWER "${ADDON}" ADDON)
    message(STATUS "Using add on board ${ADDON}")
    target_compile_options(bbcbasic PUBLIC -include ../../boards/${ADDON}.h)
  endif ()
  if ((${MIN_STACK} STREQUAL "X") OR (${MIN_STACK} STREQUAL "x"))
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
  # Registers r10 and r11 are defined as BASIC Program and Stack pointers in BBC.h
  # target_compile_options(bbcbasic PUBLIC -ffixed-r10 -ffixed-r11)
  # Work around a bug in panic.c in v2.0.0 Pico SDK
  if("${PICO_SDK_VERSION_MAJOR}.${PICO_SDK_VERSION_MINOR}.${PICO_SDK_VERSION_REVISION}" STREQUAL "2.0.0")
    target_compile_options(bbcbasic PUBLIC -include pico/platform/compiler.h)
  endif()
  target_compile_options(bbcbasic PRIVATE ${OPTIMISE} -g)
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
    hardware_resets
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
    pico_rand
    pico_stdio
    pico_stdio_uart
    pico_stdlib
    pico_sync
    pico_time
    pico_unique_id
    pico_util
    )
  if (${PICO_CHIP} STREQUAL "rp2350")
    target_compile_definitions(bbcbasic PUBLIC
      PICO=2
      PICO_INCLUDE_RTC_DATETIME
      )
    target_link_libraries(bbcbasic
      hardware_powman
      hardware_sha256
      pico_aon_timer
      pico_sha256
      )
  else()
    target_compile_definitions(bbcbasic PUBLIC
      PICO=1
      )
    target_link_libraries(bbcbasic
      hardware_rtc
      )
  endif()
  target_compile_definitions(bbcbasic PUBLIC
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
    string (TOUPPER "${CYW43}" CYW43)
    if( "${CYW43}" STREQUAL "NONE" )
      message(STATUS "No Pico W support")
      set(OS_SIZE 24)
    elseif( "${CYW43}" STREQUAL "GPIO" )
      message(STATUS "Pico W GPIO (LED) only support")
      target_link_libraries(bbcbasic
        pico_cyw43_arch_none
        )
      target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=1)
      set(OS_SIZE 24)
    elseif( "${CYW43}" STREQUAL "POLL" )
      message(STATUS "Pico W CYW43 poll support")
      target_sources(bbcbasic PRIVATE ../../src/pico/network_cyw43.c)
      target_link_libraries(bbcbasic
        pico_cyw43_arch_lwip_poll
        pico_lwip
        pico_lwip_nosys)
      target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=2)
      set(OS_SIZE 72)
    elseif( CYW43 )
      message(STATUS "Pico W CYW43 background support")
      target_sources(bbcbasic PRIVATE ../../src/pico/network_cyw43.c)
      target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=3)
      if (${NET_HEAP} GREATER 0)
        set(OS_SIZE 34)
        add_subdirectory(../lwip ${CMAKE_CURRENT_BINARY_DIR}/lwip)
        target_sources(bbcbasic PRIVATE ../../src/heap.c)
        target_compile_definitions(bbcbasic PUBLIC NET_HEAP=${NET_HEAP})
        target_link_libraries(bbcbasic
          pico_cyw43_arch_threadsafe_background
          picobb_lwip
          picobb_lwip_nosys)
      else()
        set(OS_SIZE 72)
        target_link_libraries(bbcbasic
          pico_cyw43_arch_lwip_threadsafe_background
          pico_lwip
          pico_lwip_nosys)
      endif()
    else()
      message (FATAL_ERROR "Unrecognised CYW43 support: ${CYW43}")
    endif()
  else()
    set(OS_SIZE 24)
    target_compile_definitions(bbcbasic PUBLIC HAVE_CYW43=0)
  endif()

  string (TOUPPER "${SOUND}" SOUND)
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
      ../../src/pico/sound_sdl.c
      )
    target_link_libraries(bbcbasic hardware_pwm)
  elseif ( SOUND AND (NOT "${SOUND}" STREQUAL "NONE"))
    message (FATAL_ERROR "Unrecognised sound support: ${SOUND}")
  endif()

  if ( ${LFS} )
    message(STATUS "Including LFS filesystem for flash storage")
    target_compile_definitions(bbcbasic PUBLIC
      -DHAVE_LFS
      )
    if(${PICO_CHIP} STREQUAL "rp2350")
      target_compile_definitions(bbcbasic PUBLIC
        -DROOT_SIZE=0x100000
        -DROOT_OFFSET=0x300000
        )
    else()
      target_compile_definitions(bbcbasic PUBLIC
        -DROOT_SIZE=0x100000
        -DROOT_OFFSET=0x100000
        )
    endif()
    # Set location of storage used by Bluetooth
    string(CONCAT flash_expr "1024*(" ${LFS_ORIG} "-8)")
    math(EXPR flash_orig "${flash_expr}" OUTPUT_FORMAT HEXADECIMAL)
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_FLASH_BANK_TOTAL_SIZE=0x2000               # = 2 * FLASH_SECTOR_SIZE
      -DPICO_FLASH_BANK_STORAGE_OFFSET=${flash_orig}    # = ROOT_OFFSET - PICO_FLASH_BANK_TOTAL_SIZE
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

  if( "${GRAPH}" STREQUAL "VGA" )
    message(STATUS "VGA Graphics Output")
    target_sources(bbcbasic PRIVATE
      ../../src/fbufvdu.c
      ../../src/fbufctl.c
      ../../src/pico/periodic.c
      ../../src/pico_gui/picofbuf.c
      ../../src/pico_gui/framebuffer.S
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      )
    if ( NOT "${STDIO}" STREQUAL "PICO" )
      target_compile_definitions(bbcbasic PUBLIC -DPICO_GRAPH=${GRAPH})
    endif ()
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=402
      -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
      )
    target_link_libraries(bbcbasic
      pico_scanvideo_dpi
      hardware_interp
      )
    if ( "${SOUND}" STREQUAL "SDL")
      message(FATAL_ERROR "Can not use core 1 for both sound and video")
    endif()
  elseif ( "${GRAPH}" STREQUAL "WSLCD35" )
    message(STATUS "Waveshare 3.5 inch LCD Graphics Output")
    target_sources(bbcbasic PRIVATE
      ../../src/fbufvdu.c
      ../../src/fbufctl.c
      ../../src/pico/periodic.c
      ../../src/wslcd35/DEV_Config.c
      ../../src/wslcd35/LCD_Driver.c
      ../../src/wslcd35/LCD_Touch.c
      ../../src/wslcd35/wslcd35.c
      )
    target_compile_definitions(bbcbasic PUBLIC
      -DVDU_OUT=wslcd35_out
      -DVDU_OUT_INT=wslcd35_out_int
      -DVDU_SCROLL=wslcd35_scroll
      -DVDU_OUT7=wslcd35_out7
      -DVDU_SCROLL7=wslcd35_scroll7
      -DSD_CLAIM=SPI_Claim
      -DSD_RELEASE=SPI_Release
      -DHAVE_MOUSE=1
      )
    if ( NOT "${STDIO}" STREQUAL "PICO" )
      target_compile_definitions(bbcbasic PUBLIC -DPICO_GRAPH=${GRAPH})
    endif ()
    target_link_libraries(bbcbasic
      hardware_gpio
      hardware_spi
      )
  elseif ( GRAPH AND (NOT "${GRAPH}" STREQUAL "NONE") )
    message(FATAL_ERROR "Unsupported graphical display ${GRAPH}")
  endif()
  
  if ( "${STDIO}" STREQUAL "PICO" )
    message(STATUS "BBC Basic console I/O will be on USB keyboard and ${GRAPH} screen")
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
      ../../src/pico_gui/picokbd.c
      )
    target_compile_definitions(bbcbasic PRIVATE
      -DPICO_GUI
      -DGUI_SERIAL=${GUI_SERIAL}
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      ../../include/pico_gui
      )
    target_link_libraries(bbcbasic
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
      target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB)
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

  if(${PICO_CHIP} STREQUAL "rp2350")
    set(MEM_SIZE 520)
    set(LINK_FILE picobb_2350.ld)
  else()
    set(MEM_SIZE 264)
    set(LINK_FILE picobb_2040.ld)
  endif()

  if (NOT ("${OS_RAM}" STREQUAL "" ))
    message(STATUS "Link with custom ${OS_RAM}kB of OS RAM storage")
    set(OS_SIZE ${OS_RAM})
  elseif (( "${STDIO}" STREQUAL "PICO" ) OR ( GRAPH ))
    string(CONCAT stk_expr ${OS_SIZE} "+64")
    math(EXPR OS_SIZE "${stk_expr}")
  endif()
  
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/picobb.ld
    COMMAND ${CMAKE_SOURCE_DIR}/memmap.py ${CMAKE_BINARY_DIR}/picobb.ld ${CMAKE_SOURCE_DIR}/${LINK_FILE} ${MEM_SIZE} ${OS_SIZE}
    )
  add_custom_target(link_file ALL DEPENDS ${CMAKE_BINARY_DIR}/picobb.ld)
  add_dependencies(bbcbasic link_file)
  pico_set_linker_script(bbcbasic ${CMAKE_BINARY_DIR}/picobb.ld)
  string(CONCAT stk_expr "1024*(" ${MEM_SIZE} "-" ${OS_SIZE} "-2)")
  math(EXPR stk_size "${stk_expr}" OUTPUT_FORMAT HEXADECIMAL)
  target_compile_definitions(bbcbasic PUBLIC
    PICO_STACK_SIZE=${stk_size}
    PICO_CORE1_STACK_SIZE=0x800
    )

  target_compile_definitions(bbcbasic PUBLIC -DUSB_CON=${USB_CON})
  if (${USB_CON} GREATER_EQUAL 4)
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1
      )
  endif()
  
  pico_add_extra_outputs(bbcbasic)
elseif(PICO_ON_DEVICE)
  message(WARNING "Not building bbcbasic because TinyUSB submodule is not initialized in the SDK")
endif()
