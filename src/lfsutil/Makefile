CFLAGS=-O2 -Wall -I.. -I../../pico-filesystem/littlefs -I../../include
TARGETS=mklfsimage uf2conv uf2merge
SOURCES=mklfsimage.c lfsmcu.c ../../pico-filesystem/littlefs/liblfs.a

all: $(TARGETS)

mklfsimage: $(SOURCES)
	gcc $(CFLAGS) -o mklfsimage $(SOURCES)

../../pico-filesystem/littlefs/liblfs.a:
	cd ../../pico-filesystem/littlefs && make

uf2conv: uf2conv.c uf2format.h
	gcc $(CFLAGS) -o uf2conv uf2conv.c uf2format.h

uf2merge: uf2merge.c uf2format.h
	gcc $(CFLAGS) -o uf2merge uf2merge.c

clean:
	rm -f $(TARGETS)
