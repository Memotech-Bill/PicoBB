/* pfs_fat.c - A PFS filesystem using LFS on the Pico flash memory */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include <ff.h>             // Include this before PFS header files to avoid conflicting DIR definitions
#include <pfs_private.h>

struct pfs_file *fat_open (struct pfs_pfs *pfs, const char *fn, int oflag);
int fat_close (struct pfs_file *pfs_fd);
int fat_read (struct pfs_file *pfs_fd, char *buffer, int length);
int fat_write (struct pfs_file *pfs_fd, char *buffer, int length);
long fat_lseek (struct pfs_file *pfs_fd, long pos, int whence);
int fat_fstat (struct pfs_file *pfs_fd, struct stat *buf);
int fat_isatty (struct pfs_file *fd);
int fat_stat (struct pfs_pfs *pfs, const char *name, struct stat *buf);
int fat_rename (struct pfs_pfs *pfs, const char *old, const char *new);
int fat_delete (struct pfs_pfs *pfs, const char *name);
int fat_mkdir (struct pfs_pfs *pfs, const char *pathname, mode_t mode);
int fat_rmdir (struct pfs_pfs *pfs, const char *pathname);
void *fat_opendir (struct pfs_pfs *pfs, const char *name);
struct dirent *fat_readdir (void *dirp);
int fat_closedir (void *dirp);
int fat_chmod (struct pfs_pfs *pfs, const char *pathname, mode_t mode);

static const struct pfs_v_pfs fat_v_pfs =
    {
    fat_open,
    fat_stat,
    fat_rename,
    fat_delete,
    fat_mkdir,
    fat_rmdir,
    fat_opendir,
    fat_chmod
    };
    
struct pfs_v_file fat_v_file =
    {
    fat_close,
    fat_read,
    fat_write,
    fat_lseek,
    fat_fstat,
    };

struct pfs_v_dir fat_v_dir =
    {
    fat_readdir,
    fat_closedir,
    };

struct fat_pfs
    {
    const struct pfs_v_pfs *    entry;
    FATFS                       vol;
    };

struct fat_file
    {
    const struct pfs_v_file *   entry;
    struct fat_pfs *            fat;
    const char *                pn;
    FIL                         fil;
    };

struct fat_dir
    {
    const struct pfs_v_dir *    entry;
    struct fat_pfs *            fat;
    int                         flags;
    struct pfs_mount *          m;
    struct dirent               de;
    DIR                         dir;
    };

int fat_error (FRESULT r)
    {
    switch (r)
        {
        case FR_OK:                     pfs_error (0);          break;  /* (0) Succeeded */                                                        
        case FR_DISK_ERR:               pfs_error (EBUSY);      break;  /* (1) A hard error occurred in the low level disk I/O layer */            
        case FR_INT_ERR:                pfs_error (EBUSY);      break;  /* (2) Assertion failed */                                                 
        case FR_NOT_READY:              pfs_error (EBUSY);      break;  /* (3) The physical drive cannot work */                                   
        case FR_NO_FILE:                pfs_error (ENOENT);     break;  /* (4) Could not find the file */                                          
        case FR_NO_PATH:                pfs_error (ENOENT);     break;  /* (5) Could not find the path */                                          
        case FR_INVALID_NAME:           pfs_error (EINVAL);     break;  /* (6) The path name format is invalid */                                  
        case FR_DENIED:                 pfs_error (EACCES);     break;  /* (7) Access denied due to prohibited access or directory full */         
        case FR_EXIST:                  pfs_error (EEXIST);     break;  /* (8) Access denied due to prohibited access */                           
        case FR_INVALID_OBJECT:         pfs_error (EINVAL);     break;  /* (9) The file/directory object is invalid */                             
        case FR_WRITE_PROTECTED:        pfs_error (EACCES);     break;  /* (10) The physical drive is write protected */                           
        case FR_INVALID_DRIVE:          pfs_error (EINVAL);     break;  /* (11) The logical drive number is invalid */                             
        case FR_NOT_ENABLED:            pfs_error (EINVAL);     break;  /* (12) The volume has no work area */                                     
        case FR_NO_FILESYSTEM:          pfs_error (EINVAL);     break;  /* (13) There is no valid FAT volume */                                    
        case FR_MKFS_ABORTED:           pfs_error (EBUSY);      break;  /* (14) The f_mkfs() aborted due to any problem */                         
        case FR_TIMEOUT:                pfs_error (EBUSY);      break;  /* (15) Could not get a grant to access the volume within defined period */
        case FR_LOCKED:                 pfs_error (EACCES);     break;  /* (16) The operation is rejected according to the file sharing policy */  
        case FR_NOT_ENOUGH_CORE:        pfs_error (ENOMEM);     break;  /* (17) LFN working buffer could not be allocated */                       
        case FR_TOO_MANY_OPEN_FILES:    pfs_error (ENFILE);     break;  /* (18) Number of open files > FF_FS_LOCK */                               
        case FR_INVALID_PARAMETER:      pfs_error (EINVAL);     break;  /* (19) Given parameter is invalid */                                      
        }
    return ( r == FR_OK ) ? 0 : -1;
    }

struct pfs_file *fat_open (struct pfs_pfs *pfs, const char *fn, int oflag)
    {
    struct fat_pfs *fat = (struct fat_pfs *) pfs;
    struct fat_file *fd = (struct fat_file *) malloc (sizeof (struct fat_file));
    if ( fd == NULL )
        {
        pfs_error (ENOMEM);
        return NULL;
        }
    fd->entry = &fat_v_file;
    fd->fat = fat;
    unsigned char of = 0;
    switch ( oflag & O_ACCMODE )
        {
        case O_RDONLY: of = FA_READ; break;
        case O_WRONLY: of = FA_WRITE; break;
        case O_RDWR:   of = FA_READ | FA_WRITE; break;
        }
    if ( oflag & O_APPEND ) of |= FA_OPEN_APPEND;
    if ( oflag & O_CREAT )  of |= FA_OPEN_ALWAYS;
    if ( oflag & O_TRUNC )  of |= FA_CREATE_ALWAYS;
    FRESULT r = f_open (&fd->fil, fn, of);
    if ( r == FR_OK )
        {
        return (struct pfs_file *) fd;
        }
    free (fd);
    fat_error (r);
    return NULL;
    }

int fat_close (struct pfs_file *pfs_fd)
    {
    struct fat_file *fd = (struct fat_file *) pfs_fd;
    return fat_error (f_close (&fd->fil));
    }

int fat_read (struct pfs_file *pfs_fd, char *buffer, int length)
    {
    struct fat_file *fd = (struct fat_file *) pfs_fd;
    UINT nread;
    FRESULT r = fat_error (f_read (&fd->fil, buffer, length, &nread));
    return ( r == FR_OK ) ? nread : fat_error (r);
    }

int fat_write (struct pfs_file *pfs_fd, char *buffer, int length)
    {
    struct fat_file *fd = (struct fat_file *) pfs_fd;
    UINT nwrite;
    FRESULT r = f_write (&fd->fil, buffer, length, &nwrite);
    return ( r == FR_OK ) ? nwrite : fat_error (r);
    }

long fat_lseek (struct pfs_file *pfs_fd, long pos, int whence)
    {
    struct fat_file *fd = (struct fat_file *) pfs_fd;
    switch (whence)
        {
        case SEEK_CUR: whence += f_tell (&fd->fil); break;
        case SEEK_END: whence += f_size (&fd->fil); break;
        }
    FRESULT r = f_lseek (&fd->fil, whence);
    return ( r == FR_OK ) ? f_tell (&fd->fil) : fat_error (r);
    }

int fat_fstat (struct pfs_file *pfs_fd, struct stat *buf)
    {
    return fat_stat (pfs_fd->pfs, pfs_fd->pn, buf);
    }

int fat_isatty (struct pfs_file *fd)
    {
    return 0;
    }

int fat_stat (struct pfs_pfs *pfs, const char *name, struct stat *buf)
    {
    struct fat_pfs *fat = (struct fat_pfs *) pfs;
    FILINFO info;
    FRESULT r = f_stat (name, &info);
    if ( r != FR_OK ) return fat_error (r);
    memset (buf, 0, sizeof (struct stat));
    buf->st_size = info.fsize;
    buf->st_blksize = 512;
    buf->st_blocks = info.fsize / 512;
    buf->st_nlink = 1;
    buf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
    if ( info.fattrib & AM_DIR ) buf->st_mode |= S_IFDIR;
    else buf->st_mode |= S_IFREG;
    return 0;
    }
    

int fat_rename (struct pfs_pfs *pfs, const char *old, const char *new)
    {
    return fat_error (f_rename (old, new));
    }

int fat_delete (struct pfs_pfs *pfs, const char *name)
    {
    return fat_error (f_unlink (name));
    }

int fat_mkdir (struct pfs_pfs *pfs, const char *name, mode_t mode)
    {
    return fat_error (f_mkdir (name));
    }

int fat_rmdir (struct pfs_pfs *pfs, const char *name)
    {
    return fat_error (f_unlink (name));
    }

void *fat_opendir (struct pfs_pfs *pfs, const char *name)
    {
    struct fat_pfs *fat = (struct fat_pfs *) pfs;
    struct fat_dir *dd = (struct fat_dir *) malloc (sizeof (struct fat_dir));
    if ( dd == NULL )
        {
        pfs_error (ENOMEM);
        return NULL;
        }
    dd->entry = &fat_v_dir;
    dd->fat = fat;
    FRESULT r = f_opendir (&dd->dir, name);
    if ( r == FR_OK ) return (void *) dd;
    free (dd);
    fat_error (r);
    return NULL;
    }

struct dirent *fat_readdir (void *dirp)
    {
    struct fat_dir *dd = (struct fat_dir *) dirp;
    FILINFO info;
    FRESULT r = f_readdir (&dd->dir, &info);
    if ( r != FR_OK )
        {
        fat_error (r);
        return NULL;
        }
    if ( info.fname[0] == '\0' ) return NULL;
    strncpy (dd->de.d_name, info.fname, NAME_MAX);
    return &dd->de;
    }

int fat_closedir (void *dirp)
    {
    struct fat_dir *dd = (struct fat_dir *) dirp;
    return fat_error (f_closedir (&dd->dir));
    }

int fat_chmod (struct pfs_pfs *pfs, const char *name, mode_t mode)
    {
    return pfs_error (EINVAL);
    }

struct pfs_pfs *pfs_fat_create (void)
    {
    struct fat_pfs *fat = (struct fat_pfs *) malloc (sizeof (struct fat_pfs));
    if ( fat == NULL ) return NULL;
    fat->entry = &fat_v_pfs;
    FRESULT r = f_mount (&fat->vol, "0:", 1);
    if ( r != FR_OK )
        {
        free (fat);
        fat_error (r);
        return NULL;
        }
    return (struct pfs_pfs *) fat;
    }
