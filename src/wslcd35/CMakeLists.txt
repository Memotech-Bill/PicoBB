add_library (wslcd35 INTERFACE
  DEV_Config.c
  LCD_Driver.c
  wslcd35.c
  )
target_include_directories (wslcd35 INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries (wslcd35 INTERFACE
  hardware_gpio
  hardware_spi
  )
