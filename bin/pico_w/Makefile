# Build defaults: Override by options on the make command
BOARD=vgaboard_sd_w
CYW43=BACKGROUND
LFS=Y
FAT=Y
SOUND=PWM
SERIAL_DEV=0
PRINTER=N
MIN_STACK=Y
SUFFIX=_pwg

# Required for GUI build
STDIO=PICO

# Select files loaded into LFS
LFS_FILES=../../bin/pico/examples/* ../../console/pico_w/examples/* ../../console/pico_w/examples/lib/*
LFS_EXTRA=../../bin/pico/examples ../../console/pico_w/examples

include ../../src/pico/Makefile
