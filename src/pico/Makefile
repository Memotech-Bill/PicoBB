BUILD_DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))/build
BBC_SRC=../../BBCSDL

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
    $(BBC_SRC)/src/bbdata_arm_32.s \
	$(BBC_SRC)/include/BBC.h \
	$(BBC_SRC)/include/version.h \
	$(BBC_SRC)/include/bbccon.h \
    ../../src/bbccos.c \
    ../../src/bbpico.c \
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
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=$(STDIO) -DLFS=$(LFS) -DFAT=$(FAT) -DSOUND=$(SOUND) \
			-DSTACK_CHECK=4 -DMIN_STACK=$(MIN_STACK) -DPRINTER=$(PRINTER) -DSERIAL_DEV=$(SERIAL_DEV) \
			-DCYW43=$(CYW43) -DBBC_SRC=$(BBC_SRC) -DGRAPH=$(GRAPH) --no-warn-unused-cli -S ../../../src/pico -B .

ifeq ($(BBC_SRC), ../../BBCSDL)
$(BBC_SRC)/include/version.h:
	git submodule update --remote
endif

../../littlefs/lfs.c:
	git submodule update --remote

bbcbasic$(SUFFIX).uf2 bbcbasic$(SUFFIX).elf : $(SOURCES) build/Makefile
	cd build && make
	../../src/pico/symbols/map_funcs.py build/bbcbasic.elf.map build/symbols_map.txt
	../../src/pico/symbols/merge_funcs.py build/sympico.h build/symbols_sdk.txt build/symbols_map.txt
	cd build && make
	cp build/bbcbasic.uf2 bbcbasic$(SUFFIX).uf2
	cp build/bbcbasic.elf bbcbasic$(SUFFIX).elf

ifeq ($(LFS), Y)
../../src/lfsutil/uf2merge: ../../src/lfsutil/uf2merge.c
	cd ../../src/lfsutil && make uf2merge

bbcbasic+filesystem$(SUFFIX).uf2: bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).uf2 ../../src/lfsutil/uf2merge
	../../src/lfsutil/uf2merge bbcbasic+filesystem$(SUFFIX).uf2 bbcbasic$(SUFFIX).uf2 filesystem$(SUFFIX).uf2

filesystem$(SUFFIX).uf2: ../../BBCSDL/lib/*.bbc \
		$(SDL_EXAMPLES) \
		$(LFS_FILES)
	cd ../../src/lfsutil && make clean
	cd ../../src/lfsutil && make SDL_EXAMPLES="$(SDL_EXAMPLES)" LFS_FILES="$(LFS_FILES)" \
		EXTRA="$(LFS_EXTRA)" FILESYS=$(FILESYS)
	cp ../../src/lfsutil/filesystem.uf2 filesystem$(SUFFIX).uf2
endif

clean:
	rm -rf build
	rm -f bbcbasic*.* filesystem*.* *.uf2
	cd ../../src/lfsutil && make clean
