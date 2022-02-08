ifeq ($(MIN_STACK), Y)
EXEC_SOURCES = \
    ../../src/bbexec2.c \
    ../../src/bbeval2.c
else
EXEC_SOURCES = \
    ../../BBCSDL/src/bbexec.c \
    ../../BBCSDL/src/bbeval.c
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
    ../../BBCSDL/src/sound_sdl.c
endif

ifeq ($(STDIO), PICO)
GUI_SOURCES = \
    ../../include/pico_gui/tusb_config.h \
    ../../include/font_tt.h \
    ../../src/fbufvdu.c \
    ../../src/pico_gui/picokbd.c \
    ../../src/pico_gui/picofbuf.c \
    ../../src/pico_gui/framebuffer.S
else
GUI_SOURCES =
endif

SOURCES= \
    ../../BBCSDL/src/bbmain.c \
    ../../BBCSDL/src/bbdata_arm_32.s \
	../../BBCSDL/include/BBC.h \
	../../BBCSDL/include/version.h \
	../../BBCSDL/include/bbccon.h \
    ../../src/bbccos.c \
    ../../src/bbpico.c \
    ../../src/bbasmb_arm_v6m.c \
    ../../src/lfswrap.c \
    ../../src/fault.c \
    ../../src/pico/sympico.c \
	../../src/pico/sympico.i \
    ../../src/pico/picoser.c \
	../../src/pico/stack_trap.c \
	../../src/pico/bbc_pico.ld \
	../../include/ffconf.h \
	../../include/lfswrap.h \
	../../include/sconfig.h \
	../../include/bbuart.h \
    ../../m0FaultDispatch/m0FaultDispatch.c \
    $(EXEC_SOURCES) \
    $(LFS_SOURCES) \
    $(FAT_SOURCES) \
    $(SND_SOURCES) \
    $(GUI_SOURCES)

ifeq ($(LFS), Y)
all: bbcbasic filesystem
else
all: bbcbasic
endif

bbcbasic: bbcbasic.uf2
filesystem:	filesystem.uf2

build/Makefile: ../../src/pico/CMakeLists.txt Makefile ../../src/pico/Makefile
	rm -rf build
	mkdir -p build
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=$(STDIO) -DLFS=$(LFS) -DFAT=$(FAT) -DSOUND=$(SOUND) \
			-DSTACK_CHECK=4 -DMIN_STACK=$(MIN_STACK) -S ../../../src/pico -B .

../../BBCSDL/include/version.h:
	git submodule update --remote

../../littlefs/lfs.c:
	git submodule update --remote

bbcbasic.uf2: build/Makefile $(SOURCES)
	cd build && make
	cp build/bbcbasic.uf2 bbcbasic.uf2
	cp build/bbcbasic.elf bbcbasic.elf

../../src/pico/sympico.i: ../../src/pico/sympico.sh
	cd ../../src/pico && ./sympico.sh

ifeq ($(LFS), Y)
filesystem.uf2:
	cd ../../src/lfsutil && make clean
	cd ../../src/lfsutil && make EXTRA=$(LFS_EXTRA)
	cp ../../src/lfsutil/filesystem.uf2 ../../console/pico
	cp ../../src/lfsutil/filesystem.elf ../../console/pico
endif

clean:
	rm -rf build
	rm -f bbcbasic.* filesystem.*
	cd ../../src/lfsutil && make clean
