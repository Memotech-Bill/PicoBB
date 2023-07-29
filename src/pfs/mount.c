// mount.c - Configure filesystems for Pico

#include <pfs.h>
#include <ffs_pico.h>
#include <picoser.h>

#define ROOT_SIZE 0x100000
#define ROOT_OFFSET 0x100000

void mount (void)
    {
    struct pfs_pfs *pfs;
    pfs_init ();
#ifdef HAVE_LFS
    struct lfs_config cfg;
    ffs_pico_createcfg (&cfg, ROOT_OFFSET, ROOT_SIZE);
    pfs = pfs_ffs_create (&cfg);
    pfs_mount (pfs, "/");
#ifdef HAVE_FAT
    pfs = pfs_fat_create ();
    pfs_mount (pfs, "/sdcard");
#endif
#else
#ifdef HAVE_FAT
    pfs = pfs_fat_create ();
    pfs_mount (pfs, "/");
#endif
#endif
#if SERIAL_DEV
    pfs = pfs_ser_create ();
    pfs_mount (pfs, "/dev");
#endif
    }
