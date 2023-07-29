/* pfs_stdio.c - Implementation of stdin, stdout & stderr for PFS */

#include <stdlib.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <pico/stdio.h>
#include <pfs_private.h>

int sio_close (struct pfs_file *fd);
int sio_read (struct pfs_file *fd, char *buffer, int length);
int sio_write (struct pfs_file *fd, char *buffer, int length);
long sio_lseek (struct pfs_file *fd, long pos, int whence);
int sio_fstat (struct pfs_file *fd, struct stat *buf);
int sio_isatty (struct pfs_file *fd);

static const struct pfs_v_file sio_v_file =
    {
    sio_close,
    sio_read,
    sio_write,
    sio_lseek,
    sio_fstat,
    sio_isatty
    };

struct pfs_file * pfs_stdio (int fd)
    {
    struct pfs_file *f = (struct pfs_file *) malloc (sizeof (struct pfs_file));
    if ( f == NULL ) return NULL;
    f->entry = &sio_v_file;
    return f;
    }

int sio_close (struct pfs_file *fd)
    {
    return 0;
    }

int sio_read (struct pfs_file *fd, char *buffer, int length)
    {
    // return stdio_get_until (buffer, length, at_the_end_of_time);
    for (int i = 0; i < length; ++i)
        {
        int ch = PICO_ERROR_TIMEOUT;
        while (ch == PICO_ERROR_TIMEOUT)
            {
            ch = getchar_timeout_us (0xFFFFFFFF);
            }
        buffer[i] = (char) ch;
        }
    return length;
    }

int sio_write (struct pfs_file *fd, char *buffer, int length)
    {
    // stdio_put_string (buffer, length, false, false);
    for (int i = 0; i < length; ++i) putchar_raw (buffer[i]);
    return length;
    }

long sio_lseek (struct pfs_file *fd, long pos, int whence)
    {
    return pfs_error (EINVAL);
    }

int sio_fstat (struct pfs_file *fd, struct stat *buf)
    {
    return pfs_error (EINVAL);
    }

int sio_isatty (struct pfs_file *fd)
    {
    return 1;
    }
