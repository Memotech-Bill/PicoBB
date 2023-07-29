/* pfs.h - Private include file for PFS */

#ifndef PFS_PRIVATE_H
#define PFS_PRIVATE_H

#include <dirent.h>
#include <sys/stat.h>
#include <pfs.h>

struct pfs_pfs;
struct pfs_file;
struct pfs_dir;

struct pfs_v_pfs
    {
    struct pfs_file * (*open)(struct pfs_pfs *pfs, const char *fn, int oflag);
    int (*stat)(struct pfs_pfs *pfs, const char *name, struct stat *buf);
    int (*rename)(struct pfs_pfs *pfs, const char *old, const char *new);
    int (*delete)(struct pfs_pfs *pfs, const char *name);
    int (*mkdir)(struct pfs_pfs *pfs, const char *pathname, mode_t mode);
    int (*rmdir)(struct pfs_pfs *pfs, const char *pathname);
    void *(*opendir)(struct pfs_pfs *pfs, const char *name);
    int (*chmod)(struct pfs_pfs *pfs, const char *pathname, mode_t mode);
    };

struct pfs_pfs
    {
    const struct pfs_v_pfs *    entry;
    };

struct pfs_mount
    {
    struct pfs_mount *          next;
    struct pfs_pfs *            pfs;
    const char *                moved;
    int                         nlen;
    char                        name[];
    };

struct pfs_v_file
    {
    int (*close)(struct pfs_file *fd);
    int (*read)(struct pfs_file *fd, char *buffer, int length);
    int (*write)(struct pfs_file *fd, char *buffer, int length);
    long (*lseek)(struct pfs_file *fd, long pos, int whence);
    int (*fstat)(struct pfs_file *fd, struct stat *buf);
    int (*isatty) (struct pfs_file *fd);
    };

struct pfs_file
    {
    const struct pfs_v_file *   entry;
    struct pfs_pfs *            pfs;
    const char *                pn;
    };

struct pfs_v_dir
    {
    struct dirent *(*readdir)(void *dirp);
    int (*closedir)(void *dirp);
    };

#define PFS_DF_DOT      0x01
#define PFS_DF_DDOT     0x02
#define PFS_DF_DEV      0x04
#define PFS_DF_ROOT     0x80

struct pfs_dir
    {
    const struct pfs_v_dir *    entry;
    struct pfs_pfs *            pfs;
    int                         flags;
    const struct pfs_mount *    m;
    struct dirent               de;
    };

int pfs_error (int ierr);
struct pfs_file *pfs_stdio (int fd);

#endif
