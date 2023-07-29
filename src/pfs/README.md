#General file Input / Output for the Pico SDK

This folder contains code for an experimental implementation of
file input and output (`fprintf (...)` etc.) using the hooks in the
NEWLIB stdio code.

At present it is implemented as part of a branch of Pico BBC BASIC
code. This is just for testing. It is intended to separate it out
into a separate CMake IMPORT library, as soon as I can work out the
CMake syntax for doing so.

The code has been written as far as possible to be general purpose.
It can support multiple file systems. At present, drivers have been
written for LFS filesystems stored in the Pico flash memory, and
FAT filesystems stored on an SD card.

For the most common case of a single LFS filesystem in flash memory
include the following code near the start of a program:

````
    #include <pfs.h>
    struct pfs_pfs *pfs;
    struct lfs_config cfg;
    pfs_init ();
    ffs_pico_createcfg (&cfg, ROOT_OFFSET, ROOT_SIZE);
    pfs = pfs_ffs_create (&cfg);
    pfs_mount (pfs, "/");
````

ROOT_OFFSET and ROOT_SIZE specify the position and size of the LFS
file system in the Pico flash memory.

One filesystem may be mounted as root, as shown above. Additional
filesystems may be mounted at named mount points immediately below
root, for example as "/sdcard". In terms of parsing file names it
would not be difficult to allow filesystems to be mounted in
sub-level folders. What is difficult is making such mount points
appear in directory listings.

Note that even `printf (...)` uses the NEWLIB hooks, so as a minimum
`pfs_init ()` must be called before attempting to print to `stdout`
or read from `stdin`.

A couple of issues arose during the implementation:

1. The version of NEWLIB that gets installed on Raspberry Pi OS
   appears to have been built without `HAVE_RENAME` being
   defined. As a consequence of this, NEWLIB implements `rename`
   as a call to `_link` to create a new hard link to the file,
   followed by `_unlink` to remove the old link. Of course,
   neither LFS or FAT filesystems support hard links. Therefore,
   `_link` has to be implemented as a rename operation, and
   `_unlink` has to be fudged so that it does not error owing
   to the old file name no longer existing. This makes `rename`
   work, but it means you do not get the expected error if you
   actually attempt to create a hard link.

2. It is necessary to provide an override (replacement) for
   the NEWLIB version of "dirent.h". Otherwise, the NEWLIB
   standard version includes "sys/dirent.h". That, in turn,
   includes the following line which will cause a compilation
   failure:

````
#error "<dirent.h> not supported"
````