BUILD_DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))/build
BBC_SRC=../../BBCSDL
USB_CON=0

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
    ../../src/pico_gui/picokbd.c \
    ../../src/pico_gui/picofbuf.c \
    ../../src/pico_gui/framebuffer.S
SDK_HEADERS = ../../src/pico/symbols/sdk_headers.txt
else
GUI_SOURCES = ../../src/pico/zmodem.c
SDK_HEADERS = ../../src/pico/symbols/sdk_headers.txt \
	../../src/pico/symbols/sdk_stdio_usb.txt
endif
MAP_FILE := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))/build/bbcbasic.elf.map

NET_SOURCES =
ifeq ($(CYW43), GPIO)
SDK_HEADERS +=  ../../src/pico/symbols/sdk_cyw43_gpio.txt
else ifeq ($(CYW43), POLL)
SDK_HEADERS += ../../src/pico/symbols/sdk_cyw43.txt \
	../../src/pico/symbols/sdk_cyw43_poll.txt \
	../../src/pico/symbols/sdk_lwip.txt
NET_SOURCES = ../../include/network.h \
	../../src/pico/network_cyw43.c
SDL_EXAMPLES = ../../BBCSDL/examples/general/client.bbc \
	../../BBCSDL/examples/general/server.bbc
else ifeq ($(CYW43), BACKGROUND)
SDK_HEADERS += ../../src/pico/symbols/sdk_cyw43.txt \
	../../src/pico/symbols/sdk_cyw43_background.txt \
	../../src/pico/symbols/sdk_lwip.txt
NET_SOURCES = ../../include/network.h \
	../../src/pico/network_cyw43.c
SDL_EXAMPLES = ../../BBCSDL/examples/general/client.bbc \
	../../BBCSDL/examples/general/server.bbc
else ifeq ($(CYW43), Y)
SDK_HEADERS += ../../src/pico/symbols/sdk_cyw43.txt \
	../../src/pico/symbols/sdk_cyw43_background.txt \
	../../src/pico/symbols/sdk_lwip.txt
NET_SOURCES = ../../include/network.h \
	../../src/pico/network_cyw43.c
SDL_EXAMPLES = ../../BBCSDL/examples/general/client.bbc \
	../../BBCSDL/examples/general/server.bbc
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
	../../src/pico/bbc_pico.ld \
	../../include/ffconf.h \
	../../include/lfswrap.h \
	../../include/sconfig.h \
	../../include/bbuart.h \
	../../include/crctab.h \
	../../include/zmodem.h \
    ../../m0FaultDispatch/m0FaultDispatch.c \
	build/sympico.h \
	build/pico_stub.c \
    $(EXEC_SOURCES) \
    $(LFS_SOURCES) \
    $(FAT_SOURCES) \
    $(SND_SOURCES) \
    $(GUI_SOURCES) \
	$(DEV_SOURCES) \
	$(NET_SOURCES)

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
bbcbasic: build bbcbasic$(SUFFIX).uf2 bbcbasic$(SUFFIX).elf

.PHONY: filesystem
filesystem:	bbcbasic+filesystem$(SUFFIX).uf2 filesystem$(SUFFIX).uf2

build: Makefile ../../src/pico/Makefile ../../src/pico/CMakeLists.txt
	rm -rf build
	mkdir -p build

build/sympico.h build/pico_stub.c: build $(SDK_HEADERS)
	cd ../../src/pico && symbols/header_funcs.py $(BUILD_DIR)/symbols_sdk.txt $(BUILD_DIR)/pico_stub.c \
		$(SDK_HEADERS)
	../../src/pico/symbols/merge_funcs.py build/sympico.h build/symbols_sdk.txt

build/Makefile: build build/sympico.h build/pico_stub.c
	rm -rf build/picobb.ld
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=$(STDIO) -DLFS=$(LFS) -DFAT=$(FAT) -DSOUND=$(SOUND) \
			-DSTACK_CHECK=4 -DMIN_STACK=$(MIN_STACK) -DPRINTER=$(PRINTER) -DSERIAL_DEV=$(SERIAL_DEV) \
			-DCYW43=$(CYW43) -DBBC_SRC=$(BBC_SRC) -DGRAPH=$(GRAPH) -DOS_RAM=$(OS_RAM) -DUSB_CON=$(USB_CON) \
			--no-warn-unused-cli -S ../../../src/pico -B .

ifeq ($(BBC_SRC), ../../BBCSDL)
$(BBC_SRC)/include/version.h:
	git submodule update --remote --recursive
endif

../../littlefs/lfs.h:
	git submodule update --remote --recursive

bbcbasic$(SUFFIX).uf2 bbcbasic$(SUFFIX).elf : $(SOURCES) build/Makefile
	cd build && make
	../../src/pico/symbols/map_funcs.py build/bbcbasic.elf.map build/symbols_map.txt
	../../src/pico/symbols/merge_funcs.py build/sympico.h build/symbols_sdk.txt build/symbols_map.txt
	cd build && make
	cp build/bbcbasic.uf2 bbcbasic$(SUFFIX).uf2
	cp build/bbcbasic.elf bbcbasic$(SUFFIX).elf

ifeq ($(LFS), Y)
bbcbasic+filesystem$(SUFFIX).uf2: bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).uf2 ../../src/lfsutil/uf2merge
	../../src/lfsutil/uf2merge bbcbasic+filesystem$(SUFFIX).uf2 bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).uf2

../../src/lfsutil/uf2merge: ../../src/lfsutil/uf2merge.c
	cd ../../src/lfsutil && make uf2merge

filesystem$(SUFFIX).uf2: filesystem$(SUFFIX).lfs ../../src/lfsutil/uf2conv
	../../src/lfsutil/uf2conv filesystem$(SUFFIX).lfs filesystem$(SUFFIX).uf2 0x10100000

../../src/lfsutil/uf2conv: ../../src/lfsutil/uf2conv.c
	cd ../../src/lfsutil && make uf2conv

#filesystem.elf: filesystem.lfs
#	arm-none-eabi-objcopy --change-addresses 0x10100000 --input-target=binary --output-target=elf32-little filesystem.lfs filesystem.elf

ifdef FILESYS
filesystem$(SUFFIX).lfs: $(FILESYS) ../../src/lfsutil/mklfsimage
	rm -f filesystem.lfs
	./mklfsimage -o filesystem$(SUFFIX).lfs $(FILESYS)
else
filesystem$(SUFFIX).lfs: $(BASIC_EXAMPLES) ../../src/lfsutil/pico_examples.py ../../src/lfsutil/mklfsimage
	rm -rf build/tree
	../../src/lfsutil/pico_examples.py -o filesystem$(SUFFIX).lfs -t build/tree $(DEVICE) $(BUILD) $(BASIC_EXAMPLES)
endif

../../src/lfsutil/mklfsimage: ../../src/lfsutil/mklfsimage.c
	cd ../../src/lfsutil && make mklfsimage
endif

clean:
	rm -rf build
	rm -f bbcbasic*.* filesystem*.* *.uf2
	cd ../../src/lfsutil && make clean
