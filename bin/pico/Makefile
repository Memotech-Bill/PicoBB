# Select build type
BOARD ?= pico_w
TYPE ?= $(BOARD)
ADDON ?= vgaboard_sd.h

# Build defaults: Override by options on the make command
ifeq ($(TYPE), pico)
# Pico without network support
BOARD ?= pico
CYW43 ?= NONE
STDIO ?= USB
LFS ?= Y
FAT ?= Y
SOUND ?= PWM
SERIAL_DEV ?= 0
MIN_STACK ?= Y
SUFFIX = _gui_$(BOARD)_$(ADDON)
# BASIC examples selection
DEVICE = -d all -d pico
BUILD = -b all -b console

else ifeq ($(TYPE), pico_w)
# Pico W with network support
BOARD ?= pico_w
CYW43 ?= BACKGROUND
NET_HEAP ?= 1
STDIO ?= USB
LFS ?= Y
FAT ?= Y
SOUND ?= PWM
SERIAL_DEV ?= 0
MIN_STACK ?= Y
SUFFIX ?= _gui_$(BOARD)_$(ADDON)
# BASIC examples selection
DEVICE  ?=  -d all -d pico_w
BUILD  ?=  -b all -b console

else ifeq ($(TYPE), pico2)
# Pico2  without network support
BOARD ?= pico
CYW43 ?= NONE
STDIO ?= USB
LFS ?= Y
FAT ?= Y
SOUND ?= PWM
SERIAL_DEV ?= 0
MIN_STACK ?= Y
SUFFIX = _gui_$(BOARD)_$(ADDON)
# BASIC examples selection
DEVICE = -d all -d pico
BUILD = -b all -b console

else ifeq ($(TYPE), pico2_w)
# Pico W with network support
BOARD ?= pico2_w
CYW43 ?= BACKGROUND
NET_HEAP ?= 1
STDIO ?= USB
LFS ?= Y
FAT ?= Y
SOUND ?= PWM
SERIAL_DEV ?= 0
MIN_STACK ?= Y
SUFFIX ?= _gui_$(BOARD)_$(ADDON)
# BASIC examples selection
DEVICE  ?=  -d all -d pico_w
BUILD  ?=  -b all -b console

else
$(error $(TYPE) is not a valid board type)

endif

# Required for GUI build
STDIO=PICO

include ../../src/pico/Makefile
