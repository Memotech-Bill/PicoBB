// dirent.h - Direcctory routines

#ifndef PFS_DIRENT_H
#define PFS_DIRENT_H

#include <sys/types.h>

#ifndef FF_DEFINED
// fatfs has its own definition of DIR
#define DIR void
#endif

struct dirent {
    ino_t          d_ino;       /* Inode number */
    off_t          d_off;       /* Not an offset */
    unsigned short d_reclen;    /* Length of this record */
    unsigned char  d_type;      /* Type of file; not supported by all filesystem types */
    char           d_name[256]; /* Null-terminated filename */
};

DIR *opendir (const char *name);
struct dirent *readdir (DIR *dirp);
int closedir (DIR *dirp);

#endif
