/* pfs_serial.c - A PFS filesystem for serial devices */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sys/syslimits.h>
#include <pico.h>
#include <pfs_private.h>
#include <picoser.h>

#if SERIAL_DEV > 0
#include <picoser.h>
#elif ( ! defined(PICO_DEFAULT_UART_RX_PIN) ) && ( ! defined(PICO_DEFAULT_UART_TX_PIN) )
#error No default UART defined
#endif
#if SERIAL_DEV == 1
#define UART    ( 1 - PICO_DEFAULT_UART )
#endif

typedef enum {
#if SERIAL_DEV == -1
    didStdio,
#elif SERIAL_DEV == 1
    didUart,
#elif SERIAL_DEV == 2
    didUart0,
    didUart1,
#endif
    didCount
    } DEVID;

static const char *psDevName[] = {
#if SERIAL_DEV == -1
    "serial",       // didStdio
#elif SERIAL_DEV == 1
    "uart",         // didUart
#elif SERIAL_DEV == 2
    "uart0",        // didUart0
    "uart1",        // didUart1
#endif
    };

struct pfs_file *ser_open (struct pfs_pfs *pfs, const char *fn, int oflag);
int ser_close (struct pfs_file *pfs_fd);
int ser_read (struct pfs_file *pfs_fd, char *buffer, int length);
int ser_write (struct pfs_file *pfs_fd, char *buffer, int length);
long ser_lseek (struct pfs_file *pfs_fd, long pos, int whence);
int ser_fstat (struct pfs_file *pfs_fd, struct stat *buf);
int ser_isatty (struct pfs_file *fd);
int ser_stat (struct pfs_pfs *pfs, const char *name, struct stat *buf);
int ser_rename (struct pfs_pfs *pfs, const char *old, const char *new);
int ser_delete (struct pfs_pfs *pfs, const char *name);
int ser_mkdir (struct pfs_pfs *pfs, const char *pathname, mode_t mode);
int ser_rmdir (struct pfs_pfs *pfs, const char *pathname);
void *ser_opendir (struct pfs_pfs *pfs, const char *name);
struct dirent *ser_readdir (void *dirp);
int ser_closedir (void *dirp);
int ser_chmod (struct pfs_pfs *pfs, const char *pathname, mode_t mode);

static const struct pfs_v_pfs ser_v_pfs =
    {
    ser_open,
    ser_stat,
    ser_rename,
    ser_delete,
    ser_mkdir,
    ser_rmdir,
    ser_opendir,
    ser_chmod
    };
    
struct pfs_v_file ser_v_file =
    {
    ser_close,
    ser_read,
    ser_write,
    ser_lseek,
    ser_fstat,
    };

struct pfs_v_dir ser_v_dir =
    {
    ser_readdir,
    ser_closedir,
    };

struct ser_pfs
    {
    const struct pfs_v_pfs *    entry;
    };

struct ser_file
    {
    const struct pfs_v_file *   entry;
    struct ser_pfs *            ser;
    const char *                pn;
    DEVID                       did;
    };

struct ser_dir
    {
    const struct pfs_v_dir *    entry;
    struct ser_pfs *            lfs;
    int                         flags;
    struct pfs_mount *          m;
    struct dirent               de;
    DEVID               did;
    };

struct pfs_file *ser_open (struct pfs_pfs *pfs, const char *dn, int oflag)
    {
    struct ser_pfs *ser = (struct ser_pfs *) pfs;
    struct ser_file *fd = (struct ser_file *) malloc (sizeof (struct ser_file));
    if ( fd == NULL )
        {
        pfs_error (ENOMEM);
        return NULL;
        }
    fd->entry = &ser_v_file;
    for (int did = 0; did < didCount; ++did)
        {
        if ( ! strncmp (&dn[1], psDevName[did], strlen (psDevName[did])) )
            {
            fd->did = did;
            SERIAL_CONFIG sc;
            if ( parse_sconfig (dn + strlen (psDevName[did]) + 1, &sc) )
                {
                switch (did)
                    {
#if SERIAL_DEV == -1
                    case didStdio:
                        if ( sc.baud > 0 ) uart_set_baudrate (uart_default, sc.baud);
                        uart_set_format (uart_default, sc.data, sc.stop, sc.parity);
                        return (struct pfs_file *) fd;
#elif SERIAL_DEV == 1
                    case didUart:
                        if ( uopen (UART, &sc) ) return (struct pfs_file *) fd;
                        break;
#elif SERIAL_DEV == 2
                    case didUart0:
                        if ( uopen (0, &sc) ) return (struct pfs_file *) fd;
                        break;
                    case didUart1:
                        if ( uopen (1, &sc) ) return (struct pfs_file *) fd;
                        break;
#endif
                    }
                }
            pfs_error (EINVAL);
            return NULL;
            }
        }
    pfs_error (ENXIO);
    free (fd);
    return NULL;
    }

int ser_close (struct pfs_file *pfs_fd)
    {
    struct ser_file *fd = (struct ser_file *) pfs_fd;
    switch (fd->did)
        {
#if SERIAL_DEV == -1
        case didStdio:
            break;
#elif SERIAL_DEV == 1
        case didUart:
            uclose (UART);
            break;
#elif SERIAL_DEV == 2
        case didUart0:
            uclose (0);
            break;
        case didUart1:
            uclose (1);
            break;
#endif
        }
    return 0;
    }

int ser_read (struct pfs_file *pfs_fd, char *buffer, int length)
    {
    struct ser_file *fd = (struct ser_file *) pfs_fd;
    switch (fd->did)
        {
#if SERIAL_DEV == -1
        case didStdio:
            return fread (buffer, 1, length, stdin);
#elif SERIAL_DEV == 1
        case didUart:
            return uread (buffer, 1, length, UART);
#elif SERIAL_DEV == 2
        case didUart0:
            return uread (buffer, 1, length, 0);
        case didUart1:
            return uread (buffer, 1, length, 1);
#endif
        }
    return 0;
    }

int ser_write (struct pfs_file *pfs_fd, char *buffer, int length)
    {
    struct ser_file *fd = (struct ser_file *) pfs_fd;
    switch (fd->did)
        {
#if SERIAL_DEV == -1
        case didStdio:
            return fwrite (buffer, 1, length, stdout);
#elif SERIAL_DEV == 1
        case didUart:
            return uwrite (buffer, 1, length, UART);
#elif SERIAL_DEV == 2
        case didUart0:
            return uwrite (buffer, 1, length, 0);
        case didUart1:
            return uwrite (buffer, 1, length, 1);
#endif
        }
    return 0;
    }

long ser_lseek (struct pfs_file *pfs_fd, long pos, int whence)
    {
    return pfs_error (EINVAL);
    }

int ser_fstat (struct pfs_file *pfs_fd, struct stat *buf)
    {
    return pfs_error (EINVAL);
    }

int ser_isatty (struct pfs_file *fd)
    {
    return 0;
    }

int ser_stat (struct pfs_pfs *pfs, const char *name, struct stat *buf)
    {
    return pfs_error (EINVAL);
    }
    

int ser_rename (struct pfs_pfs *pfs, const char *old, const char *new)
    {
    return pfs_error (EINVAL);
    }

int ser_delete (struct pfs_pfs *pfs, const char *name)
    {
    return pfs_error (EINVAL);
    }

int ser_mkdir (struct pfs_pfs *pfs, const char *pathname, mode_t mode)
    {
    return pfs_error (EINVAL);
    }

int ser_rmdir (struct pfs_pfs *pfs, const char *pathname)
    {
    return pfs_error (EINVAL);
    }

void *ser_opendir (struct pfs_pfs *pfs, const char *name)
    {
    struct ser_pfs *ser = (struct ser_pfs *) pfs;
    struct ser_dir *dd = (struct ser_dir *) malloc (sizeof (struct ser_dir));
    if ( dd == NULL )
        {
        pfs_error (ENOMEM);
        return NULL;
        }
    dd->entry = &ser_v_dir;
    dd->did = 0;
    return (void *) dd;
    }

struct dirent *ser_readdir (void *dirp)
    {
    struct ser_dir *dd = (struct ser_dir *) dirp;
    if ( dd->did >= didCount ) return NULL;
    strncpy (dd->de.d_name, psDevName[dd->did], NAME_MAX);
    ++dd->did;
    return &dd->de;
    }

int ser_closedir (void *dirp)
    {
    return 0;
    }

int ser_chmod (struct pfs_pfs *pfs, const char *pathname, mode_t mode)
    {
    return pfs_error (EINVAL);
    }

struct pfs_pfs *pfs_ser_create (void)
    {
    struct ser_pfs *ser = (struct ser_pfs *) malloc (sizeof (struct ser_pfs));
    if ( ser == NULL ) return NULL;
    ser->entry = &ser_v_pfs;
    return (struct pfs_pfs *) ser;
    }
