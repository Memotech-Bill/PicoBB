set(SDK_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/sdk_headers.txt
  )

if(NOT ("${STDIO}" STREQUAL "PICO"))
  list(APPEND SDK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_stdio_usb.txt
    )
endif()

if (${PICO_CHIP} STREQUAL "rp2350")
  list(APPEND SDK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_2350.txt
    )
else()
  list(APPEND SDK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_2040.txt
    )
endif()

if("${CYW43}" STREQUAL "GPIO")
  list(APPEND SDK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_cyw43_gpio.txt
    )
elseif("${CYW43}" STREQUAL "POLL")
  list(APPEND SDK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_cyw43.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_cyw43_poll.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_lwip.txt
    )
elseif("${CYW43}" STREQUAL "NONE")
elseif(CYW43)
  list(APPEND SDK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_cyw43.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_cyw43_background.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_lwip.txt
    )
endif()

message(STATUS "Custom command to build ${CMAKE_BINARY_DIR}/pico_stub.c")
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/pico_stub.c
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/header_funcs.py
    ${CMAKE_BINARY_DIR}/symbols_sdk.txt
    ${CMAKE_BINARY_DIR}/pico_stub.c
    ${SDK_HEADERS}
  DEPENDS ${SDK_HEADERS}
  )
add_custom_target(stub_file ALL DEPENDS ${CMAKE_BINARY_DIR}/pico_stub.c)

message(STATUS "Custom command to build ${CMAKE_BINARY_DIR}/sympico.h")
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/sympico.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/merge_funcs.py
    ${CMAKE_BINARY_DIR}/sympico.h
    ${CMAKE_BINARY_DIR}/symbols_sdk.txt
    ${CMAKE_BINARY_DIR}/symbols_map.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/library_functions.txt
  DEPENDS
    stub_file
    ${CMAKE_BINARY_DIR}/symbols_sdk.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/library_functions.txt
  )
add_custom_target(sym_file ALL DEPENDS ${CMAKE_BINARY_DIR}/sympico.h)
# add_dependencies(bbcbasic sym_file)
