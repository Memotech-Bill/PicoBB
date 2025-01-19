# Select build type
BOARD ?= pico_w
TYPE ?= $(BOARD)
ADDON ?=

# Build defaults: Override by options on the make command
ifeq ($(TYPE), pico)
# Pico without network support
BOARD ?= pico
CYW43 ?= NONE
# BASIC examples selection
DEVICE = -d all -d pico

else ifeq ($(TYPE), pico_w)
# Pico W with network support
BOARD ?= pico_w
CYW43 ?= BACKGROUND
NET_HEAP ?= 1
# BASIC examples selection
DEVICE  ?=  -d all -d pico_w

else ifeq ($(TYPE), pico2)
# Pico2  without network support
BOARD ?= pico
CYW43 ?= NONE
# BASIC examples selection
DEVICE = -d all -d pico

else ifeq ($(TYPE), pico2_w)
# Pico W with network support
BOARD ?= pico2_w
CYW43 ?= BACKGROUND
# BASIC examples selection
DEVICE  ?= -d all -d pico_w

else
$(error $(TYPE) is not a valid board type)

endif

STDIO ?= USB+UART
LFS ?= Y
FAT ?= N
SOUND ?= SDL
SERIAL_DEV ?= 1
MIN_STACK ?= Y

# BASIC examples selection
BUILD = -b all -b console

ifneq ($(ADDON),)
SUFFIX = _console_$(TYPE)_$(ADDON)
DEVICE += -d $(ADDON)
else
SUFFIX = _console_$(TYPE)
endif

include ../../src/pico/Makefile
