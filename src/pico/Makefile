BUILD_DIR = build$(SUFFIX)
BUILD_PATH := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))/$(BUILD_DIR)
ADDON?=
BBC_SRC?=../../BBCSDL
USB_CON?=0
NET_HEAP?=0
STACK_CHECK?=4
LFS_ORIG?=1024
LFS_SIZE?=1024

EXAMPLE_FILES = ../../BBCSDL/lib/*.bbc \
		../../BBCSDL/tests/*.bbc \
		../../BBCSDL/examples/general/*.bbc \
		../../console/pico/examples/* \
		../../console/pico/examples/lib/*

ifeq ($(MIN_STACK), Y)
EXEC_SOURCES = \
    ../../src/bbexec2.c \
    ../../src/bbeval2.c
else
EXEC_SOURCES = \
    $(BBC_SRC)/src/bbexec.c \
    $(BBC_SRC)/src/bbeval.c
endif

ifeq ($(LFS), Y)
LFS_SOURCES = \
	../../include/lfsmcu.h \
    ../../littlefs/lfs.h \
    ../../littlefs/lfs_util.h \
    ../../src/lfsmcu.c \
    ../../littlefs/lfs.c \
    ../../littlefs/lfs_util.c
else
LFS_SOURCES =
endif

ifeq ($(FAT), Y)
FAT_SOURCES = \
    ../../fatfs/ff.h \
    ../../fatfs/diskio.h \
	../../fatfs/ff.c \
	../../fatfs/ffsystem.c \
	../../fatfs/ffunicode.c \
    ../../include/pico/sd_spi.h \
    ../../include/ffconf.h \
	../../src/pico/ff_disk.c \
	../../src/pico/sd_spi2.c \
	../../src/pico/sd_spi.pio
else
FAT_SOURCES =
endif

ifeq ($(SOUND), I2S)
SND_SOURCES = \
    ../../src/pico/sound.pio \
    ../../src/sn76489.c \
    ../../src/pico/pico_snd.c
else ifeq ($(SOUND), PWM)
SND_SOURCES = \
    ../../src/sn76489.c \
    ../../src/pico/pico_snd.c
else ifeq ($(SOUND), SDL)
SND_SOURCES = \
    $(BBC_SRC)/src/sound_sdl.c
endif

ifeq ($(STDIO), PICO)
GUI_SOURCES = \
    ../../include/pico_gui/tusb_config.h \
    ../../include/font_tt.h \
    ../../src/fbufvdu.c \
    ../../src/fbufctl.c \
    ../../src/pico_gui/picokbd.c \
    ../../src/pico_gui/picofbuf.c \
    ../../src/pico_gui/framebuffer.S \
	../../include/fbufvdu.h
SDK_HEADERS = ../../src/pico/symbols/sdk_headers.txt
EXAMPLE_FILES += ../../bin/pico/examples/* \
		../../bin/pico/examples/graphics/*
else ifeq ($(GRAPH), VGA)
GUI_SOURCES = \
    ../../include/pico_gui/tusb_config.h \
    ../../include/font_tt.h \
    ../../src/fbufvdu.c \
    ../../src/fbufctl.c \
	../../src/pico/periodic.c \
    ../../src/pico_gui/picofbuf.c \
    ../../src/pico_gui/framebuffer.S \
	../../src/pico/zmodem.c \
	../../include/fbufvdu.h \
	../../include/periodic.h
SDK_HEADERS = ../../src/pico/symbols/sdk_headers.txt \
	../../src/pico/symbols/sdk_stdio_usb.txt
EXAMPLE_FILES += ../../bin/pico/examples/* \
		../../bin/pico/examples/graphics/*
else ifeq ($(GRAPH), WSLCD35)
GUI_SOURCES = \
    ../../include/pico_gui/tusb_config.h \
    ../../include/font_tt.h \
    ../../src/fbufvdu.c \
    ../../src/fbufctl.c \
	../../src/pico/periodic.c \
	../../src/wslcd35/DEV_Config.c \
	../../src/wslcd35/LCD_Driver.c \
	../../src/wslcd35/LCD_Touch.c \
	../../src/wslcd35/wslcd35.c \
	../../src/wslcd35/DEV_Config.h \
	../../src/wslcd35/LCD_Driver.h \
	../../src/wslcd35/LCD_Touch.h \
	../../src/pico/zmodem.c \
	../../include/fbufvdu.h \
	../../include/periodic.h
SDK_HEADERS = ../../src/pico/symbols/sdk_headers.txt \
	../../src/pico/symbols/sdk_stdio_usb.txt
EXAMPLE_FILES += ../../bin/pico/examples/* \
		../../bin/pico/examples/graphics/*
else
GUI_SOURCES = ../../src/pico/zmodem.c
SDK_HEADERS = ../../src/pico/symbols/sdk_headers.txt \
	../../src/pico/symbols/sdk_stdio_usb.txt
endif
MAP_FILE := $(BUILD_PATH)/bbcbasic.elf.map

NET_SOURCES =
ifeq ($(CYW43), GPIO)
SDK_HEADERS +=  ../../src/pico/symbols/sdk_cyw43_gpio.txt
else ifeq ($(CYW43), POLL)
SDK_HEADERS += ../../src/pico/symbols/sdk_cyw43.txt \
	../../src/pico/symbols/sdk_cyw43_poll.txt \
	../../src/pico/symbols/sdk_lwip.txt
NET_SOURCES = ../../include/network.h \
	../../src/pico/network_cyw43.c \
	../../src/heap.c \
	../../src/lwip/memp.c \
	../../include/lwipopts.h \
	../../include/heap.h \
	../../include/network.h
SDL_EXAMPLES = ../../BBCSDL/examples/general/client.bbc \
	../../BBCSDL/examples/general/server.bbc
EXAMPLE_FILES += 	../../console/pico/examples_w/* \
		../../console/pico/examples_w/lib/*
else ifeq ($(CYW43), BACKGROUND)
SDK_HEADERS += ../../src/pico/symbols/sdk_cyw43.txt \
	../../src/pico/symbols/sdk_cyw43_background.txt \
	../../src/pico/symbols/sdk_lwip.txt
NET_SOURCES = ../../include/network.h \
	../../src/pico/network_cyw43.c \
	../../src/heap.c \
	../../src/lwip/memp.c
SDL_EXAMPLES = ../../BBCSDL/examples/general/client.bbc \
	../../BBCSDL/examples/general/server.bbc
EXAMPLE_FILES += 	../../console/pico/examples_w/* \
		../../console/pico/examples_w/lib/*
else ifeq ($(CYW43), Y)
SDK_HEADERS += ../../src/pico/symbols/sdk_cyw43.txt \
	../../src/pico/symbols/sdk_cyw43_background.txt \
	../../src/pico/symbols/sdk_lwip.txt
NET_SOURCES = ../../include/network.h \
	../../src/pico/network_cyw43.c \
	../../src/heap.c \
	../../src/lwip/memp.c
SDL_EXAMPLES = ../../BBCSDL/examples/general/client.bbc \
	../../BBCSDL/examples/general/server.bbc
EXAMPLE_FILES += 	../../console/pico/examples_w/* \
		../../console/pico/examples_w/lib/*
endif

ifneq ($(SERIAL_DEV), 0)
DEV_SOURCES = \
	../../src/pico/picoser.c \
	../../include/bbuart.h
else
DEV_SOURCES =
endif

SOURCES= \
    $(BBC_SRC)/src/bbmain.c \
    $(BBC_SRC)/src/bbccos.c \
    $(BBC_SRC)/src/bbdata_arm_32.s \
	$(BBC_SRC)/include/BBC.h \
	$(BBC_SRC)/include/version.h \
	$(BBC_SRC)/include/bbccon.h \
    ../../src/bbpico.c \
    ../../src/picocli.c \
    ../../src/bbasmb_arm_v6m.c \
    ../../src/lfswrap.c \
    ../../src/fault.c \
    ../../src/pico/sympico.c \
    ../../src/pico/picoser.c \
	../../src/pico/stack_trap.c \
	../../src/pico/crctab.c \
	../../src/pico/picobb_2040.ld \
	../../src/pico/picobb_2350.ld \
	../../include/fbufvdu.h \
	../../include/ffconf.h \
	../../include/lfswrap.h \
	../../include/sconfig.h \
	../../include/bbuart.h \
	../../include/crctab.h \
	../../include/picocos.h \
	../../include/zmodem.h \
    ../../m0FaultDispatch/m0FaultDispatch.c \
    $(EXEC_SOURCES) \
    $(LFS_SOURCES) \
    $(FAT_SOURCES) \
    $(SND_SOURCES) \
    $(GUI_SOURCES) \
	$(DEV_SOURCES) \
	$(NET_SOURCES)
#	$(BUILD_DIR)/sympico.h \
#	$(BUILD_DIR)/pico_stub.c \

ifneq ("$(wildcard ../../BBCSDL/pico_examples.cfg)", "")
BASIC_EXAMPLES = ../../pico_examples.cfg ../../BBCSDL/pico_examples.cfg
else
BASIC_EXAMPLES = ../../pico_examples.cfg 
endif

OS_RAM =

.PHONY: all
ifeq ($(LFS), Y)
all: bbcbasic filesystem
else
all: bbcbasic
endif

.PHONY: bbcbasic
bbcbasic: $(BUILD_DIR)/Makefile bbcbasic$(SUFFIX).uf2 bbcbasic$(SUFFIX).elf

.PHONY: filesystem
filesystem:	bbcbasic+filesystem$(SUFFIX).uf2 filesystem$(SUFFIX).uf2

$(BUILD_DIR)/Makefile: Makefile ../../src/pico/Makefile ../../src/pico/CMakeLists.txt
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DADDON=$(ADDON) -DSTDIO=$(STDIO) -DLFS=$(LFS) -DFAT=$(FAT) -DSOUND=$(SOUND) \
			-DSTACK_CHECK=$(STACK_CHECK) -DMIN_STACK=$(MIN_STACK) -DPRINTER=$(PRINTER) -DSERIAL_DEV=$(SERIAL_DEV) \
			-DCYW43=$(CYW43) -DBBC_SRC=$(BBC_SRC) -DGRAPH=$(GRAPH) -DOS_RAM=$(OS_RAM) -DUSB_CON=$(USB_CON) \
			-DNET_HEAP=$(NET_HEAP) -DLFS_ORIG=$(LFS_ORIG) -DOPTIMISE=$(OPTIMISE) --no-warn-unused-cli -S ../../../src/pico -B .

ifeq ($(BBC_SRC), ../../BBCSDL)
$(BBC_SRC)/include/version.h:
	git submodule update --remote --recursive
endif

../../littlefs/lfs.h:
	git submodule update --remote --recursive

$(BUILD_DIR)/bbcbasic.map : $(SOURCES) $(BUILD_DIR)/Makefile
	rm -rf $(BUILD_DIR)/picobb.ld
	cd $(BUILD_DIR) && make
	cp $(BUILD_DIR)/bbcbasic.elf.map $(BUILD_DIR)/bbcbasic.map

bbcbasic$(SUFFIX).uf2 bbcbasic$(SUFFIX).elf : $(SOURCES) $(BUILD_DIR)/bbcbasic.map ../../src/pico/symbols/library_functions.txt $(BUILD_DIR)/Makefile
	../../src/pico/symbols/map_funcs.py $(BUILD_DIR)/bbcbasic.map $(BUILD_DIR)/symbols_map.txt
	rm -f $(BUILD_DIR)/sympico.h
	cd $(BUILD_DIR) && make
	cp $(BUILD_DIR)/bbcbasic.uf2 bbcbasic$(SUFFIX).uf2
	cp $(BUILD_DIR)/bbcbasic.elf bbcbasic$(SUFFIX).elf

ifeq ($(LFS), Y)
bbcbasic+filesystem$(SUFFIX).uf2: bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).uf2 ../../src/lfsutil/uf2merge
	../../src/lfsutil/uf2merge -f -o bbcbasic+filesystem$(SUFFIX).uf2 bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).uf2

../../src/lfsutil/uf2merge: ../../src/lfsutil/uf2merge.c
	cd ../../src/lfsutil && make uf2merge

filesystem$(SUFFIX).uf2: filesystem$(SUFFIX).lfs bbcbasic$(SUFFIX).uf2 ../../src/lfsutil/uf2conv
	../../src/lfsutil/uf2conv -m bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).lfs filesystem$(SUFFIX).uf2 $(LFS_ORIG)K

../../src/lfsutil/uf2conv: ../../src/lfsutil/uf2conv.c
	cd ../../src/lfsutil && make uf2conv

#filesystem.elf: filesystem.lfs
#	arm-none-eabi-objcopy --change-addresses 0x10100000 --input-target=binary --output-target=elf32-little filesystem.lfs filesystem.elf

ifdef FILESYS
filesystem$(SUFFIX).lfs: $(FILESYS) ../../src/lfsutil/mklfsimage
	rm -f filesystem.lfs
	../../src/lfsutil/mklfsimage -o filesystem$(SUFFIX).lfs $(FILESYS)
else
filesystem$(SUFFIX).lfs: $(BASIC_EXAMPLES) $(EXAMPLE_FILES) ../../src/lfsutil/pico_examples.py ../../src/lfsutil/mklfsimage
	rm -rf $(BUILD_DIR)/tree
	../../src/lfsutil/pico_examples.py -o filesystem$(SUFFIX).lfs -s $(LFS_SIZE)K -t $(BUILD_DIR)/tree $(DEVICE) $(BUILD) $(BASIC_EXAMPLES)
endif

../../src/lfsutil/mklfsimage: ../../src/lfsutil/mklfsimage.c ../../src/lfsmcu.c ../../littlefs/lfs.c ../../littlefs/lfs_util.c
	cd ../../src/lfsutil && make mklfsimage
endif

clean:
	rm -rf build*
	rm -f bbcbasic*.* filesystem*.* *.uf2
	cd ../../src/lfsutil && make clean
