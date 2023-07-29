/* pfs_base.c - An implementation of the stub routines needed to make Newlib file access routines work */

#include <stdlib.h>
#include <string.h>
#include <sys/syslimits.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <pfs_private.h>
#include <dirent.h>
#include <pname.h>

#undef errno
extern int errno;

#define STDIO_HANDLE_STDIN  0
#define STDIO_HANDLE_STDOUT 1
#define STDIO_HANDLE_STDERR 2

static struct pfs_mount *mounts = NULL;
static struct pfs_file ** files = NULL;
static int num_handle = 0;
static const char *cwd = NULL;
static char rootdir = '/';

int pfs_error (int ierr)
    {
    errno = ierr;
    return ( ierr < 0 ) ? -1 : 0;
    }

int pfs_init (void)
    {
    if ( num_handle != 0 ) return -1;
    num_handle = 3;
    files = (struct pfs_file **) malloc (num_handle * sizeof (struct pfs_file *));
    if ( files == NULL ) return -2;
    files[0] = pfs_stdio (0);
    if ( files[0] == NULL ) return -3;
    files[1] = pfs_stdio (1);
    if ( files[1] == NULL ) return -4;
    files[2] = pfs_stdio (2);
    if ( files[2] == NULL ) return -5;
    }

int pfs_mount (struct pfs_pfs *pfs, const char *psMount)
    {
    if ( pfs == NULL ) return -6;
    int nlen = strlen (psMount);
    struct pfs_mount *m = (struct pfs_mount *) malloc (sizeof (struct pfs_mount) + nlen + 2);
    if ( m == NULL ) return -7;
    m->moved = NULL;
    const char *ps1 = psMount;
    char *ps2 = m->name;
    *ps2 = '/';
    ++ps2;
    if (( *ps1 == '/' ) || ( *ps1 == '\\' )) ++ps1;
    while ( *ps1 != '\0' )
        {
        if (( *ps1 == '/' ) || ( *ps1 == '\\' ))
            {
            ++ps1;
            break;
            }
        *ps2 = *ps1;
        ++ps1;
        ++ps2;
        }
    if ( *ps1 != '\0' )
        {
        free (m);
        return -8;
        }
    *ps2 = '\0';
    if ( ps2 == m->name + 1 )
        {
        if ( mounts != NULL )
            {
            free (m);
            return -9;
            }
        --ps2;
        *ps2 = '\0';
        }
    struct pfs_mount *m2 = mounts;
    while ( m2 != NULL )
        {
        if ( strcmp (m->name, m2->name) == 0 )
            {
            free (m);
            return -10;
            }
        m2 = m2->next;
        }
    m->nlen = ps2 - m->name;
    m->pfs = pfs;
    m->next = mounts;
    mounts = m;
    if ( num_handle == 0 )
        {
        int ierr = pfs_init ();
        if ( ierr != 0 ) return ierr;
        }
    if ( cwd == NULL )
        {
        ps2 = (char *) malloc (m->nlen + 2);
        ps2[0] = '/';
        strcpy (&ps2[1], &m->name[1]);
        cwd = ps2;
        }
    return 0;
    }

int _read (int handle, char *buffer, int length)
    {
    if (( handle >= 0 ) && ( handle < num_handle ) && ( files[handle] != NULL ))
        {
        struct pfs_file *f = files[handle];
        return f->entry->read (f, buffer, length);
        }
    errno = EBADF;
    return -1;
    }

int _write (int handle, char *buffer, int length)
    {
    if (( handle >= 0 ) && ( handle < num_handle ) && ( files[handle] != NULL ))
        {
        struct pfs_file *f = files[handle];
        return f->entry->write (f, buffer, length);
        }
    errno = EBADF;
    return -1;
    }

static struct pfs_mount *reference (const char **pn, const char **pr)
    {
    if (( mounts == NULL ) || ( cwd == NULL ))
        {
        errno = ENOENT;
        return NULL;
        }
    *pn = pname_append (cwd, *pn);
    if ( *pn == NULL )
        {
        errno = ENAMETOOLONG;
        return NULL;
        }
    int nlen = strlen (*pn);
    for (struct pfs_mount *m = mounts; m != NULL; m = m->next)
        {
        if ( strncmp (*pn, m->name, m->nlen) == 0 )
            {
            if ( (*pn)[m->nlen] == '\0' )
                {
                *pr = &rootdir;
                return m;
                }
            if ( (*pn)[m->nlen] == '/' )
                {
                *pr = *pn + m->nlen;
                return m;
                }
            }
        }
    errno = ENOENT;
    return NULL;
    }

int _open (const char *fn, int oflag, ...)
    {
    const char *rn;
    struct pfs_mount *m = reference (&fn, &rn);
    if ( m == NULL ) return -1;
    struct pfs_file *f = m->pfs->entry->open (m->pfs, rn, oflag);
    if ( f == NULL )
        {
        free ((void *)fn);
        return -1;
        }
    f->pn = fn;
    for ( int fd = 0; fd < num_handle; ++fd )
        {
        if ( files[fd] == NULL )
            {
            files[fd] = f;
            return fd;
            }
        }
    int nh = 2 * num_handle;
    struct pfs_file ** fi2 = (struct pfs_file **) realloc (files, nh);
    if ( fi2 == NULL )
        {
        f->entry->close (f);
        free ((void *) f->pn);
        free (f);
        errno = ENFILE;
        return -1;
        }
    files = fi2;
    for (int fd = num_handle; fd < nh; ++fd)
        {
        files[fd] = NULL;
        }
    int fd = num_handle;
    files[fd] = f;
    num_handle = nh;
    return fd;
    }

int _close (int fd)
    {
    if (( fd >= 0 ) && ( fd < num_handle ) && ( files[fd] != NULL ))
        {
        struct pfs_file *f = files[fd];
        int ierr = f->entry->close (f);
        free ((void *)files[fd]->pn);
        free (files[fd]);
        files[fd] = NULL;
        return ierr;
        }
    errno = EBADF;
    return -1;
    }

long _lseek (int fd, long pos, int whence)
    {
    if (( fd >= 0 ) && ( fd < num_handle ) && ( files[fd] != NULL ))
        {
        struct pfs_file *f = files[fd];
        return f->entry->lseek (f, pos, whence);
        }
    errno = EBADF;
    return -1;
    }

int _fstat (int fd, struct stat *buf)
    {
    if (( fd >= 0 ) && ( fd < num_handle ) && ( files[fd] != NULL ))
        {
        struct pfs_file *f = files[fd];
        return f->entry->fstat (f, buf);
        }
    errno = EBADF;
    return -1;
    }

int _isatty (int fd)
    {
    if (( fd >= 0 ) && ( fd < num_handle ) && ( files[fd] != NULL ))
        {
        struct pfs_file *f = files[fd];
        return f->entry->isatty (f);
        }
    errno = EBADF;
    return -1;
    }

int _stat (const char *name, struct stat *buf)
    {
    int ierr = -1;
    const char *rname;
    struct pfs_mount *m = reference (&name, &rname);
    if ( m == NULL ) return -1;
    ierr = m->pfs->entry->stat (m->pfs, rname, buf);
    free ((void *)name);
    return ierr;
    }

int _link (const char *old, const char *new)
    {
    int ierr = -1;
    const char *rold;
    struct pfs_mount *m1 = reference (&old, &rold);
    if ( m1 == NULL ) return -1;
    const char *rnew;
    struct pfs_mount *m2 = reference (&new, &rnew);
    if ( m2 == NULL )
        {
        free ((void *)old);
        return -1;
        }
    if ( m2 == m1 )
        {
        ierr = m1->pfs->entry->rename (m1->pfs, rold, rnew);
        }
    if ( ierr == 0 )
        {
        if ( m1->moved != NULL ) free ((void *)m1->moved);
        m1->moved = old;
        }
    else
        {
        free ((void *)old);
        }
    free ((void *)new);
    return ierr;
    }

int _unlink (const char *name)
    {
    int ierr = -1;
    const char *rname;
    struct pfs_mount *m = reference (&name, &rname);
    if ( m == NULL ) return -1;
    ierr = m->pfs->entry->delete (m->pfs, rname);
    if ( m->moved != NULL )
        {
        if (( ierr == -1 ) && ( strcmp (m->moved, name) == 0 )) ierr = 0;
        free ((void *)m->moved);
        m->moved = NULL;
        }
    free ((void *)name);
    return ierr;
    }

int chdir (const char *path)
    {
    const char *pn = pname_append (cwd, path);
    if ( pn == NULL ) return -1;
    free ((void *)cwd);
    cwd = pn;
    return 0;
    }

int mkdir (const char *name, mode_t mode)
    {
    int ierr = -1;
    const char *rname;
    struct pfs_mount *m = reference (&name, &rname);
    if ( m == NULL ) return -1;
    ierr = m->pfs->entry->mkdir (m->pfs, rname, mode);
    free ((void *)name);
    return ierr;
    }

int rmdir (const char *name)
    {
    int ierr = -1;
    const char *rname;
    struct pfs_mount *m = reference (&name, &rname);
    if ( m == NULL ) return -1;
    ierr = m->pfs->entry->rmdir (m->pfs, rname);
    free ((void *)name);
    return ierr;
    }

char *getcwd (char *buf, size_t size)
    {
    if ( buf == NULL ) return strdup (cwd);
    if ( size < strlen (cwd) + 1 ) return NULL;
    strcpy (buf, cwd);
    return buf;
    }

void *opendir (const char *name)
    {
    const char *rname;
    struct pfs_mount *m = reference (&name, &rname);
    if ( m == NULL ) return NULL;
    struct pfs_dir *d = m->pfs->entry->opendir (m->pfs, rname);
    if ( d != NULL )
        {
        d->flags = PFS_DF_DOT;
        if ( strcmp (name, "/") == 0 )
            {
            d->flags |= PFS_DF_DEV | PFS_DF_ROOT;
            d->m = mounts;
            }
        else
            {
            d->flags |= PFS_DF_DDOT;
            }
        }
    free ((void *)name);
    return d;
    }

static bool pfs_special (const char *name)
    {
    if ( strcmp (name, ".") == 0 ) return true;
    if ( strcmp (name, "..") == 0 ) return true;
    for (struct pfs_mount *m = mounts; m != NULL; m = m->next)
        {
        if ( strcmp (name, m->name) == 0 ) return true;
        }
    return false;
    }

struct dirent *readdir (void *dirp)
    {
    struct pfs_dir *d = (struct pfs_dir *) dirp;
    memset (&d->de, 0, sizeof (struct dirent));
    if ( d->flags & PFS_DF_DOT )
        {
        strcpy (d->de.d_name, ".");
        d->flags &= ~ PFS_DF_DOT;
        return &d->de;
        }
    if ( d->flags & PFS_DF_DDOT )
        {
        strcpy (d->de.d_name, "..");
        d->flags &= ~ PFS_DF_DDOT;
        return &d->de;
        }
    if ( d->flags & PFS_DF_DEV )
        {
        strcpy (d->de.d_name, &d->m->name[1]);
        while (true)
            {
            d->m = d->m->next;
            if ( d->m == NULL )
                {
                d->flags &= ~ PFS_DF_DEV;
                break;
                }
            if ( d->m->nlen > 0 ) break;
            }
        return &d->de;
        }
    while (true)
        {
        if ( d->entry->readdir (d) == NULL ) break;
        if ( ! pfs_special (d->de.d_name) ) return &d->de;
        }
    return NULL;
    }

int closedir (void *dirp)
    {
    struct pfs_dir *d = (struct pfs_dir *) dirp;
    int ierr = d->entry->closedir (d);
    free (d);
    return ierr;
    }

int chmod (const char *name, mode_t mode)
    {
    int ierr = -1;
    const char *rname;
    struct pfs_mount *m = reference (&name, &rname);
    if ( m == NULL ) return -1;
    ierr = m->pfs->entry->chmod (m->pfs, rname, mode);
    free ((void *)name);
    return ierr;
    }

char *realpath (const char *path, char *resolved_path)
    {
    char *ps = resolved_path;
    const char *pn = pname_append (cwd, path);
    if ( resolved_path == NULL )
        {
        ps = strdup (pn);
        }
    else if ( strlen (pn) >= PATH_MAX )
        {
        ps = NULL;
        }
    else
        {
        strncpy (ps, pn, PATH_MAX - 1);
        ps[PATH_MAX - 1] = '\0';
        }
    free ((void *)pn);
    return ps;
    }
