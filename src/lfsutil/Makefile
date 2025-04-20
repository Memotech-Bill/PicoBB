CFLAGS=-O2 -Wall -I.. -I../../littlefs -I../../include
TARGETS=mklfsimage uf2conv uf2merge uf2dump lfsdump
SOURCES=mklfsimage.c ../lfsmcu.c ../../littlefs/lfs.c ../../littlefs/lfs_util.c

all: $(TARGETS)

mklfsimage: $(SOURCES)
	gcc $(CFLAGS) -g -o mklfsimage $(SOURCES)

uf2conv: uf2conv.c uf2format.h
	gcc $(CFLAGS) -g -o uf2conv uf2conv.c uf2format.h

uf2merge: uf2merge.c uf2format.h
	gcc $(CFLAGS) -g -o uf2merge uf2merge.c

uf2dump: uf2dump.c
	gcc -g -o uf2dump uf2dump.c

lfsdump: lfsdump.c ../../littlefs/bd/lfs_filebd.c ../../littlefs/lfs.c ../../littlefs/lfs_util.c
	gcc $(CFLAGS) -g -o lfsdump lfsdump.c ../../littlefs/bd/lfs_filebd.c ../../littlefs/lfs.c ../../littlefs/lfs_util.c

clean:
	rm -f $(TARGETS)
