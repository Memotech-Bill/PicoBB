/*  fswrap.c -- Wrappers for LittleFS and other glue.
    Written 2021 by Eric Olson
    FatFS & Device support added by Memotech-Bill

    This is free software released under the exact same terms as
    stated in license.txt for the Basic interpreter.  */

#define DEBUG   0
#if DEBUG
#if DEBUG == 2
#define dbgmsg message
void message (const char *psFmt, ...);
#else
#define dbgmsg printf
#endif
#endif

#define LFS_TELL    1   // 0 = Manually track, 1 = Use lfs_file_tell

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pico/stdlib.h>
#include <pico.h>
#include "lfswrap.h"

#ifdef HAVE_LFS
#include "lfsmcu.h"
lfs_t lfs_root;
lfs_bbc_t lfs_root_context;
extern uint32_t __flash_binary_end;
#define BINARY_END  ((uint32_t *)& __flash_binary_end)
#endif

#if defined(HAVE_FAT) && defined(HAVE_LFS)
#define	SDMOUNT	"sdcard"	// SD Card mount point
#define SDMLEN	( strlen (SDMOUNT) + 1 )
#endif
#if SERIAL_DEV != 0
#include "sconfig.h"
#define DEVMOUNT "dev"      // Device mount point
#define DEVMLEN ( strlen (DEVMOUNT) + 1 )
#if SERIAL_DEV > 0
#include "bbuart.h"
#elif ( ! defined(PICO_DEFAULT_UART_RX_PIN) ) && ( ! defined(PICO_DEFAULT_UART_TX_PIN) )
#error No default UART defined
#endif
#if SERIAL_DEV == 1
#define UART    ( 1 - PICO_DEFAULT_UART )
#endif

static bool parse_sconfig (const char *ps, SERIAL_CONFIG *sc);

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
#endif

typedef enum {
#ifdef HAVE_LFS
    fstLFS,
#endif
#ifdef HAVE_FAT
    fstFAT,
#endif
#if SERIAL_DEV != 0
    fstDEV,
#endif
    } FSTYPE;

#define flgRoot     0x01
#if SERIAL_DEV != 0
#define flgDev      0x02
#endif
#if defined (HAVE_LFS) && defined (HAVE_FAT)
#define flgFat      0x04
#endif

typedef struct
    {
    FSTYPE fstype;
    union
        {
#ifdef HAVE_LFS
        lfs_file_t lfs_ft;
#endif
#ifdef HAVE_FAT
        FIL	   fat_ft;
#endif
#if SERIAL_DEV != 0
        DEVID   did;
#endif
        };
#if defined (HAVE_LFS) && ( LFS_TELL == 0 )
    unsigned int    npos;
#endif
    } multi_file;

static inline FSTYPE get_filetype (const FILE *fp)
    {
    return ((multi_file *)fp)->fstype;
    }

static inline void set_filetype (FILE *fp, FSTYPE fst)
    {
    ((multi_file *)fp)->fstype = fst;
    }

typedef struct
    {
    FSTYPE fstype;
    int flags;
    struct dirent   de;
    union
        {
#ifdef HAVE_LFS
        lfs_dir_t   lfs_dt;
#endif
#ifdef HAVE_FAT
        DIR	    fat_dt;
#endif
#if SERIAL_DEV != 0
        DEVID   did;
#endif
        };
    } dir_info;

static inline FSTYPE get_dirtype (const DIR *dirp)
    {
    return ((dir_info *)dirp)->fstype;
    }

static inline void set_dirtype (DIR *dirp, FSTYPE fst)
    {
    ((dir_info *)dirp)->fstype = fst;
    }

static inline struct dirent * deptr (DIR *dirp)
    {
    return &((dir_info *)dirp)->de;
    }

#ifdef HAVE_LFS
static inline lfs_file_t *lfsptr (FILE *fp)
    {
    return &((multi_file *)fp)->lfs_ft;
    }

static inline lfs_dir_t * lfsdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->lfs_dt;
    }
#endif

#ifdef HAVE_FAT
static inline char *fat_path (char *path)
    {
#ifdef HAVE_LFS
    return path + SDMLEN;
#else
    return path;
#endif
    }

static inline FIL *fatptr (FILE *fp)
    {
    return &((multi_file *)fp)->fat_ft;
    }

static inline DIR * fatdir (DIR *dirp)
    {
    return &((dir_info *)dirp)->fat_dt;
    }
#endif

#if SERIAL_DEV != 0
static inline const char *dev_path (const char *path)
    {
    return path + DEVMLEN;
    }

static inline void set_devid (FILE *fp, DEVID did)
    {
    ((multi_file *)fp)->did = did;
    }

static inline DEVID get_devid (const FILE *fp)
    {
    return ((multi_file *)fp)->did;
    }
#endif

static FSTYPE pathtype (const char *path)
    {
#if SERIAL_DEV != 0
    if (( ! strncmp (path, "/"DEVMOUNT, DEVMLEN) ) &&
        ( ( path[DEVMLEN] == '/' ) || ( path[DEVMLEN] == '\0' ) ) ) return fstDEV;
#endif
#ifdef HAVE_LFS
#ifdef HAVE_FAT
    if (( ! strncmp (path, "/"SDMOUNT, SDMLEN) ) &&
        ( ( path[SDMLEN] == '/' ) || ( path[SDMLEN] == '\0' ) ) ) return fstFAT;
#endif
    return fstLFS;
#else
    return fstFAT;
#endif
    }

static int fswslash (char c)
    {
    if (c == '/' || c == '\\') return 1;
    return 0;
    }

static int fswvert (char c)
    {
    if (fswslash (c) || c == 0) return 1;
    return 0;
    }

static const char *fswedge (const char *p)
    {
    const char *q = p;
    while (*q)
        {
        if (fswslash (*q)) return q+1;
        q++;
        }
    return q;
    }

static int fswsame (const char *q, const char *p)
    {
    for (;;)
        {
        if (fswvert (*p))
            {
            if (fswvert (*q))
                {
                return 1;
                }
            else
                {
                return 0;
                }
            }
        else if (fswvert (*q))
            {
            return 0;
            }
        else if (*q != *p)
            {
            return 0;
            }
        q++; p++;
        }
    }

static void fswrem (char *pcwd)
    {
    char *p = pcwd, *q = 0;
    while (*p)
        {
        if (fswslash (*p)) q = p;
        p++;
        }
    if (q) *q = 0;
    else *pcwd = 0;
    }

static void fswadd (char *pcwd, const char *p)
    {
    char *q = pcwd;
    while (*q) q++;
    *q++ = '/'; 
    while (! (fswvert (*p))) *q++ = *p++;
    *q = 0;
    }

static char fswcwd[260] = "";
char *myrealpath (const char *restrict p, char *restrict r)
    {
    if (r == 0) r = malloc (260);
    if (r == 0) return 0;
    strcpy (r, fswcwd);
    if (fswsame (p, "/"))
        {
        *r = 0;
        if (!*p) return r;
        p++;
        }
    const char *f = fswedge (p);
    while (f-p > 0)
        {
        if (fswsame (p, "/")||fswsame (p, "./"))
            {
            }
        else if (fswsame (p, "../"))
            {
            fswrem (r);
            }
        else
            {
            fswadd (r, p);
            }
        p = f; f = fswedge (p);
        }
    return r;
    }

static char fswpath[260];
int mychdir (const char *p)
    {
    int err = 0;
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        if ( strcmp (fswpath, "/"DEVMOUNT) ) err = -1;
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_chdir (fat_path (fswpath)) != FR_OK ) err = -1;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        lfs_dir_t d;
        if (lfs_dir_open (&lfs_root, &d, fswpath) < 0) err = -1;
        lfs_dir_close (&lfs_root, &d);
        }
#endif
    if ( ! err ) strcpy (fswcwd, fswpath);
    return err;
    }

int mymkdir (const char *p, mode_t m)
    {
    (void) m;
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_mkdir (fat_path (fswpath)) != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_mkdir (&lfs_root, fswpath);
        if (r < 0) return -1;
        return 0;
        }
#endif
    return -1;
    }

int myrmdir (const char *p)
    {
    myrealpath (p, fswpath);
    FSTYPE fst = pathtype (fswpath);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_unlink (fat_path (fswpath)) != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_remove (&lfs_root, fswpath);
        if (r < 0) return -1;
        return 0;
        }
#endif
    return -1;
    }

int mychmod (const char *p, mode_t m)
    {
    return 0;
    }

char *mygetcwd (char *b, size_t s)
    {
    strncpy (b, fswcwd, s);
    return b;
    }

long myftell (FILE *fp)
    {
    FSTYPE fst = get_filetype (fp);
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        long iTell = f_tell (fatptr (fp));
#if DEBUG
        dbgmsg ("ftell (%p) fat = %d\r\n", fp, iTell);
#endif
        return iTell;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
#if LFS_TELL == 1
        int r = lfs_file_tell (&lfs_root, lfsptr (fp));
#if DEBUG
        dbgmsg ("ftell (%p) lfs = %d\r\n", fp, r);
#endif
        if (r < 0) return -1;
        return r;
#else
#if DEBUG
        dbgmsg ("ftell (%p) lfs npos = %d\r\n", fp, ((multi_file *)fp)->npos);
#endif
        return ((multi_file *)fp)->npos;
#endif
        }
#endif
    return -1;
    }

int myfseek (FILE *fp, long offset, int whence)
    {
    FSTYPE fst = get_filetype (fp);
#if DEBUG
    dbgmsg ("fseek (%p, %d, %s)\r\n", fp, offset,
        (whence == SEEK_END) ? "SEEK_END" : (whence == SEEK_CUR) ? "SEEK_CUR" : "SEEK_SET");
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FIL *pf = fatptr (fp);
        if (whence == SEEK_END) offset += f_size (pf);
        else if (whence == SEEK_CUR) offset += f_tell (pf);
        FRESULT fr = f_lseek (pf, offset);
#if DEBUG
        dbgmsg ("fseek fat offset = %d, fr = %d\r\n", offset, fr);
#endif
        if ( fr != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int lfs_whence = LFS_SEEK_SET;
#if LFS_TELL == 1
        if (whence == SEEK_END) lfs_whence = LFS_SEEK_END;
        else if (whence == SEEK_CUR) lfs_whence = LFS_SEEK_CUR;
#else
        if (whence == SEEK_CUR)
            {
#if DEBUG
            dbgmsg ("Current position = %d\r\n", ((multi_file *)fp)->npos);
#endif
            offset += ((multi_file *)fp)->npos;
            }
        else if (whence == SEEK_END)
            {
            int r = lfs_file_size (&lfs_root, lfsptr (fp));
#if DEBUG
            dbgmsg ("File size = %d\r\n", r);
#endif
            if (r < 0) return -1;
            offset += r;
            }
#endif
        int r = lfs_file_seek (&lfs_root, lfsptr (fp), offset, lfs_whence);
#if DEBUG
        dbgmsg ("fseek lfs offset = %d, r = %d\r\n", offset, r);
#endif
        if (r < 0) return -1;
#if LFS_TELL == 0
        ((multi_file *)fp)->npos = offset;
#endif
        return 0;
        }
#endif
    return -1;
    }

int myfextent (FILE *fp, long offset)
    {
    if (offset < 0) return -1;
    FSTYPE fst = get_filetype (fp);
#if DEBUG
    dbgmsg ("fextent (%p, %d)\r\n", fp, offset);
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FRESULT fr;
        FIL *pf = fatptr (fp);
        long size = f_size (pf);
        long posn = f_tell (pf);
#if DEBUG
        dbgmsg ("fsize (%p) fat = %d\r\n", fp, size);
        dbgmsg ("ftell (%p) fat = %d\r\n", fp, posn);
#endif
        if (offset <= size)
            {
            fr = f_lseek (pf, offset);
#if DEBUG
            dbgmsg ("fseek fat offset = %d, fr = %d\r\n", offset, fr);
#endif
            if ( fr != FR_OK ) return -1;
            fr = f_truncate (pf);
            if ( fr != FR_OK ) return -1;
            }
        else if (size == 0)
            {
            fr = f_expand (pf, offset, 1);
            if ( fr != FR_OK ) return -1;
            return 0;
            }
        else
            {
            fr = f_lseek (pf, offset);
            if ( fr != FR_OK ) return -1;
            }
        if (posn > offset) posn = offset;
        fr = f_lseek (pf, posn);
        if ( fr != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        lfs_file_t *pf = lfsptr (fp);
        long size = lfs_file_size (&lfs_root, pf);
        long posn = lfs_file_tell (&lfs_root, pf);
        if (offset <= size)
            {
            int r = lfs_file_truncate (&lfs_root, pf, offset);
            if (r < 0) return -1;
            if (posn > offset) posn = offset;
            r = lfs_file_seek (&lfs_root, pf, offset, LFS_SEEK_SET);
            if (r < 0) return -1;
            }
        return 0;
        }
#endif
    return -1;
    }

FILE *myfopen (const char *p, char *mode)
    {
    FILE *fp = (FILE *) malloc (sizeof (multi_file));
    if ( fp == NULL ) return NULL;
    myrealpath (p, fswpath);
#if DEBUG
    dbgmsg ("fopen (%s, %s)\r\n", fswpath, mode);
#endif
    FSTYPE fst = pathtype (fswpath);
    set_filetype (fp, fst);
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        const char *psDev = dev_path (fswpath) + 1;
        for (int did = 0; did < didCount; ++did)
            {
            if ( ! strncmp (psDev, psDevName[did], strlen (psDevName[did])) )
                {
                set_devid (fp, did);
                SERIAL_CONFIG sc;
                if ( parse_sconfig (psDev + strlen (psDevName[did]), &sc) )
                    {
                    switch (did)
                        {
#if SERIAL_DEV == -1
                        case didStdio:
                            if ( sc.baud > 0 ) uart_set_baudrate (uart_default, sc.baud);
                            uart_set_format (uart_default, sc.data, sc.stop, sc.parity);
                            return fp;
#elif SERIAL_DEV == 1
                        case didUart:
                            if ( uopen (UART, &sc) ) return fp;
                            break;
#elif SERIAL_DEV == 2
                        case didUart0:
                            if ( uopen (0, &sc) ) return fp;
                            break;
                        case didUart1:
                            if ( uopen (1, &sc) ) return fp;
                            break;
#endif
                        }
                    }
                break;
                }
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        unsigned char om = 0;
        switch (mode[0])
            {
            case 'r':
                om = FA_READ;
                if ( mode[1] == '+' ) om |= FA_WRITE;
                break;
            case 'w':
                om = FA_CREATE_ALWAYS | FA_WRITE;
                if ( mode[1] == '+' ) om |= FA_READ;
                break;
            case 'a':
                om = FA_OPEN_APPEND;
                if ( mode[1] == '+' ) om |= FA_READ;
                break;
            }
        if ( f_open (fatptr (fp), fat_path (fswpath), om) == FR_OK ) return fp;
        
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        enum lfs_open_flags of = 0;
        switch (mode[0])
            {
            case 'r':
                of = LFS_O_RDONLY;
                if ( mode[1] == '+' ) of = LFS_O_RDWR;
                break;
            case 'w':
                of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_WRONLY;
                if ( mode[1] == '+' ) of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDWR;
                break;
            case 'a':
                of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDONLY | LFS_O_APPEND;
                if ( mode[1] == '+' ) of = LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDWR | LFS_O_APPEND;
                break;
            }
        if ( lfs_file_open (&lfs_root, lfsptr (fp), fswpath, of) >= 0 )
            {
#if LFS_TELL == 0
            ((multi_file *)fp)->npos = 0;
#endif
            return fp;
            }
        }
#endif
    free (fp);
    return NULL;
    }

int myfclose (FILE *fp)
    {
#if DEBUG
    dbgmsg ("fclose (%p)\r\n", fp);
#endif
    int err = 0;
    FSTYPE fst = get_filetype (fp);
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        switch (get_devid (fp))
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
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FRESULT fr = f_close (fatptr (fp));
        err = ( fr != FR_OK ) ? -1 : 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        int r = lfs_file_close (&lfs_root, lfsptr (fp));
        err = ( r < 0 ) ? -1 : 0;
        }
#endif
    free (fp);
    return err;
    }

size_t myfread (void *ptr, size_t size, size_t nmemb, FILE *fp)
    {
    if (fp == stdin)
        {
#undef fread
        return fread (ptr, size, nmemb, stdin);
#define fread myfread
        }
#if DEBUG
    dbgmsg ("fread (%p, %d, %d, %p)\r\n", ptr, size, nmemb, fp);
#endif
    FSTYPE fst = get_filetype (fp);
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        switch (get_devid (fp))
            {
#if SERIAL_DEV == -1
            case didStdio:
                return fread (ptr, size, nmemb, stdin);
#elif SERIAL_DEV == 1
            case didUart:
                return uread (ptr, size, nmemb, UART);
#elif SERIAL_DEV == 2
            case didUart0:
                return uread (ptr, size, nmemb, 0);
            case didUart1:
                return uread (ptr, size, nmemb, 1);
#endif
            default:
                return 0;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        unsigned int nbyte;
        if (size == 1)
            {
            f_read (fatptr (fp), ptr, nmemb, &nbyte);
#if DEBUG
            dbgmsg ("fread fat, size=1, nbyte = %d\r\n", nbyte);
#endif
            return nbyte;
            }
        else
            {
            for (int i = 0; i < nmemb; ++i)
                {
                FRESULT fr = f_read (fatptr (fp), ptr, size, &nbyte);
                if ( (fr != FR_OK) || (nbyte < size))
                    {
#if DEBUG
                    dbgmsg ("fread fat, size=%d, fr = %d, nbyte = %d, i = %d\r\n", size, fr, nbyte, i);
#endif
                    return i;
                    }
                ptr += size;
                }
            }
#if DEBUG
        dbgmsg ("fread fat, size=%d, nmemb = %d\r\n", size, nmemb);
#endif
        return nmemb;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        if (size == 1)
            {
            int r = lfs_file_read (&lfs_root, lfsptr (fp), (char *)ptr, nmemb);
#if DEBUG
            dbgmsg ("fread lfs, size=1, r = %d\r\n", r);
#endif
            if (r < 0) return 0;
#if LFS_TELL == 0
            ((multi_file *)fp)->npos += r;
#endif
            return r;
            } 
        for (int i = 0; i < nmemb; ++i)
            {
            int r = lfs_file_read (&lfs_root, lfsptr (fp), (char *)ptr, size);
#if LFS_TELL == 0
            if ( r > 0 ) ((multi_file *)fp)->npos += r;
#endif
            if (r < size)
                {
#if DEBUG
                dbgmsg ("fread lfs, size=%d, i = %d\r\n", size, i);
#endif
                return i;
                }
            ptr += size;
            }
#if DEBUG
        dbgmsg ("fread lfs, size=%d, nmemb = %d\r\n", size, nmemb);
#endif
        return nmemb;
        }
#endif
    return 0;
    }

size_t myfwrite (const void *ptr, size_t size, size_t nmemb, FILE *fp)
    {
    if (fp == stdout || fp == stderr)
        {
#undef fwrite
        return fwrite (ptr, size, nmemb, stdout);
#define fwrite myfwrite
        }
#if DEBUG
    dbgmsg ("fwrite (%p, %d, %d, %p)\r\n", ptr, size, nmemb, fp);
#endif
    FSTYPE fst = get_filetype (fp);
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        switch (get_devid (fp))
            {
#if SERIAL_DEV == -1
            case didStdio:
                return fwrite (ptr, size, nmemb, stdout);
#elif SERIAL_DEV == 1
            case didUart:
                return uwrite (ptr, size, nmemb, UART);
#elif SERIAL_DEV == 2
            case didUart0:
                return uwrite (ptr, size, nmemb, 0);
            case didUart1:
                return uwrite (ptr, size, nmemb, 1);
#endif
            default:
                return 0;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        unsigned int nbyte;
        if (size == 1)
            {
            f_write (fatptr (fp), ptr, nmemb, &nbyte);
            return nbyte;
            }
        else
            {
            for (int i = 0; i < nmemb; ++i)
                {
                FRESULT fr = f_write (fatptr (fp), ptr, size, &nbyte);
                if ( (fr != FR_OK) || (nbyte < size)) return i;
                ptr += size;
                }
            }
        return nmemb;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        if (size == 1)
            {
            int r = lfs_file_write (&lfs_root, lfsptr (fp), (char *)ptr, nmemb);
            if (r < 0) return 0;
#if LFS_TELL == 0
            ((multi_file *)fp)->npos += r;
#endif
            return r;
            } 
        for (int i = 0; i < nmemb; ++i)
            {
            int r = lfs_file_write (&lfs_root, lfsptr (fp), (char *)ptr, size);
#if LFS_TELL == 0
            if ( r > 0 ) ((multi_file *)fp)->npos += r;
#endif
            if (r < size) return i;
            ptr += size;
            }
        return nmemb;
        }
#endif
    return 0;
    }

int usleep (useconds_t usec)
    {
    sleep_us (usec);
    return 0;
    }

unsigned int sleep (unsigned int seconds)
    {
    sleep_ms (seconds*1000);
    return 0;
    }

DIR *myopendir (const char *name)
    {
    DIR *dirp = (DIR *) malloc (sizeof (dir_info));
    if ( dirp == NULL ) return NULL;
    myrealpath (name, fswpath);
    FSTYPE fst = pathtype (fswpath);
    set_dirtype (dirp, fst);
    if ( fswpath[0] == '\0' ) ((dir_info *)dirp)->flags = flgRoot;
    else ((dir_info *)dirp)->flags = 0;
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        const char *psDir = dev_path (fswpath);
        if (( *psDir == '\0' ) || (( *psDir == '/' ) && ( psDir[1] == '\0' )))
            {
            ((dir_info *)dirp)->did = 0;
            return dirp;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        char *psDir = fat_path (fswpath);
        if ( *psDir == '\0' ) strcpy (psDir, "/");
        if ( f_opendir (fatdir (dirp), psDir) == FR_OK ) return dirp;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        if ( lfs_dir_open (&lfs_root, lfsdir (dirp), name) >= 0 ) return dirp;
        }
#endif
    free (dirp);
    return NULL;
    }

static bool fn_special (DIR *dirp, const char *psName)
    {
    if ( (((dir_info *)dirp)->flags & flgRoot ) == 0) return false;
    if (( ! strcmp (psName, ".") ) || ( ! strcmp (psName, "..") )) return true;
#if SERIAL_DEV != 0
    if ( ! strcmp (psName, DEVMOUNT) ) return true;
#endif
#if defined(HAVE_LFS) && defined(HAVE_FAT)
    if ( ! strcmp (psName, SDMOUNT) ) return true;
#endif
    return false;
    }

struct dirent *myreaddir (DIR *dirp)
    {
    if ( dirp == NULL ) return NULL;
    struct dirent *pde = deptr (dirp);
    FSTYPE fst = get_dirtype (dirp);
    if ( ((dir_info *)dirp)->flags & flgRoot )
        {
#if SERIAL_DEV != 0
        if ( (((dir_info *)dirp)->flags & flgDev) == 0 )
            {
            ((dir_info *)dirp)->flags |= flgDev;
            strcpy (pde->d_name, DEVMOUNT"/");
            return pde;
            }
#endif
#if defined(HAVE_LFS) && defined(HAVE_FAT)
        if ( (((dir_info *)dirp)->flags & flgFat) == 0 )
            {
            ((dir_info *)dirp)->flags |= flgFat;
            strcpy (pde->d_name, SDMOUNT"/");
            return pde;
            }
#endif
        }
#if SERIAL_DEV != 0
    if ( fst == fstDEV )
        {
        DEVID did = ((dir_info *)dirp)->did;
        if ( did < didCount )
            {
            ++((dir_info *)dirp)->did;
            strcpy (pde->d_name, psDevName[did]);
            return pde;
            }
        }
#endif
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        FILINFO fi;
        while (true)
            {
            if (( f_readdir (fatdir (dirp), &fi) != FR_OK ) || ( fi.fname[0] == '\0' )) return NULL;
            if ( ! fn_special (dirp, fi.fname) )
                {
                strncpy (pde->d_name, fi.fname, NAME_MAX);
                if (( fi.fattrib & AM_DIR ) && ( strlen (fi.fname) < NAME_MAX )) strcat (pde->d_name, "/");
                return pde;
                }
            }
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        struct lfs_info r;
        while (true)
            {
            if ( !lfs_dir_read (&lfs_root, lfsdir (dirp), &r) ) return NULL;
            if ( ! fn_special (dirp, r.name) )
                {
                strncpy (pde->d_name, r.name, NAME_MAX);
                if (( r.type == LFS_TYPE_DIR ) && ( strlen (r.name) < NAME_MAX))  strcat (pde->d_name, "/");
                return pde;
                }
            }
        }
#endif
    return NULL;
    }

int myclosedir (DIR *dirp)
    {
    if ( dirp == NULL ) return -1;
    FSTYPE fst = get_dirtype (dirp);
    int err = 0;
#ifdef HAVE_FAT
    if ( fst == fstFAT )
        {
        if ( f_closedir (fatdir (dirp)) != FR_OK ) err = -1;
        }
#endif
#ifdef HAVE_LFS
    if ( fst == fstLFS )
        {
        lfs_dir_close (&lfs_root, lfsdir (dirp));
        }
#endif
    free (dirp);
    }

extern int myrename (const char *old, const char *new)
    {
    static char newpath[260];
    myrealpath (old, fswpath);
    myrealpath (new, newpath);
    FSTYPE fsto = pathtype (fswpath);
    FSTYPE fstn = pathtype (newpath);
    if ( fstn != fsto ) return -1;
#ifdef HAVE_FAT
    if ( fstn == fstFAT )
        {
        if ( f_rename (fat_path (fswpath), fat_path (newpath)) != FR_OK ) return -1;
        return 0;
        }
#endif
#ifdef HAVE_LFS
    if ( fstn == fstLFS )
        {
        int r = lfs_rename (&lfs_root, fswpath, newpath);
        if ( r < 0 ) return -1;
        return 0;
        }
#endif
    return -1;
    }

#ifdef HAVE_LFS
static struct lfs_config lfs_root_cfg = {
    .context = &lfs_root_context,
    .read = lfs_bbc_read,
    .prog = lfs_bbc_prog,
    .erase = lfs_bbc_erase,
    .sync = lfs_bbc_sync,
    .read_size = 1,
    .prog_size = FLASH_PAGE_SIZE,
    .block_size = FLASH_SECTOR_SIZE,
    .block_count = ROOT_SIZE / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_PAGE_SIZE,
    .cache_size = 256,
    .lookahead_size = 32,
    .block_cycles = 256
    };

static const uint8_t    lfs_head[] = {0xf0, 0x0f, 0xff, 0xf7, 0x6c, 0x69, 0x74, 0x74, 0x6c, 0x65, 0x66, 0x73, 0x2f, 0xe0, 0x00, 0x10};

/*
#ifdef PICO
#include <pico/binary_info.h>
bi_decl(bi_block_device(
                           BINARY_INFO_MAKE_TAG('F', 'S'),
                           "Flash file system (LFS)",
                           XIP_BASE+ROOT_OFFSET,
                           ROOT_SIZE,
                           NULL,
                           BINARY_INFO_BLOCK_DEV_FLAG_READ | BINARY_INFO_BLOCK_DEV_FLAG_WRITE));
#endif
*/
#endif

extern void syserror (const char *psMsg);
int mount (char *psMsg)
    {
    int istat = 0;
#ifdef HAVE_LFS
#ifdef PICO
    struct lfs_bbc_config lfs_bbc_cfg;
    lfs_bbc_cfg.buffer = (void *)(((intptr_t) BINARY_END) & (~ (FLASH_SECTOR_SIZE-1)));
    while (true)
        {
        lfs_bbc_cfg.buffer += FLASH_SECTOR_SIZE;
        // printf ("buffer = %p:", lfs_bbc_cfg.buffer);
        // for (int i = 0; i < 20; ++i) printf (" %02X", *((uint8_t *)lfs_bbc_cfg.buffer+i));
        // printf ("%s", "\n");
        if (((*((uint8_t *)lfs_bbc_cfg.buffer + 4) & 0x7F) == (lfs_head[0] & 0x7F))
            && (! memcmp (lfs_bbc_cfg.buffer + 5, &lfs_head[1], 11))
            && ((*((uint8_t *)lfs_bbc_cfg.buffer + 16) & 0x7F) == (lfs_head[12] & 0x7F))
            && (! memcmp (lfs_bbc_cfg.buffer + 17, &lfs_head[13], 3))
                ) break;
        if (lfs_bbc_cfg.buffer >= (void *)(XIP_BASE + PICO_FLASH_SIZE_BYTES))
            {
            syserror ("Unable to locate LittleFS image");
            lfs_bbc_cfg.buffer = 0;
            break;
            }
        }
    if (lfs_bbc_cfg.buffer > 0)
        {
        uint32_t    lfs_ver = *((uint32_t *)(lfs_bbc_cfg.buffer + 20));
        uint32_t    lfs_bsz = *((uint32_t *)(lfs_bbc_cfg.buffer + 24));
        uint32_t    lfs_bct = *((uint32_t *)(lfs_bbc_cfg.buffer + 28));
        if (psMsg)
            {
            psMsg += strlen (psMsg);
            sprintf (psMsg, "LittleFS image v%d.%d, Size = %dKB, Origin = 0x%08X\r\n",
                lfs_ver >> 16, lfs_ver & 0xFFFF, lfs_bsz * lfs_bct / 1024, lfs_bbc_cfg.buffer);
            }
        if (lfs_ver != LFS_DISK_VERSION)
            {
            syserror ("Invalid VFS version");
            return 2;
            }
        if (lfs_bsz != FLASH_SECTOR_SIZE)
            {
            syserror ("Invalid flash sector size");
            return 2;
            }
        if (lfs_bbc_cfg.buffer + lfs_bsz * lfs_bct > (void *)(XIP_BASE + PICO_FLASH_SIZE_BYTES))
            {
            syserror ("LittleFS image too large");
            return 2;
            }
        lfs_root_cfg.block_count = lfs_bct;
        }
    else
        {
        lfs_bbc_cfg.buffer = (void *)(XIP_BASE + ROOT_OFFSET);
        lfs_root_cfg.block_count = ROOT_SIZE;
        }
#else
#error Define location of LFS storage
#endif
    lfs_bbc_createcfg (&lfs_root_cfg, &lfs_bbc_cfg);
    int lfs_err = lfs_mount (&lfs_root, &lfs_root_cfg);
    if (lfs_err)
        {
        lfs_format (&lfs_root, &lfs_root_cfg);
        lfs_err = lfs_mount (&lfs_root, &lfs_root_cfg);
        if (lfs_err)
            {
            syserror ("Unable to format littlefs.");
            istat |= 2;
            }
        }
#endif
#ifdef HAVE_FAT
    static FATFS   vol;
    FRESULT fr = f_mount (&vol, "0:", 1);
    if ( fr != FR_OK )
        {
        syserror ("Failed to mount SD card.");
        istat |= 1;
        }
#endif
    return istat;
    }

#if SERIAL_DEV != 0
static bool parse_sconfig (const char *ps, SERIAL_CONFIG *sc)
    {
    static const char *psPar[] = { "baud", "parity", "data", "stop", "tx", "rx", "cts", "rts"};
    int iPar = 0;
    memset (sc, -1, sizeof (SERIAL_CONFIG));
#if DEBUG
    dbgmsg ("parse_config (%s)\r\n", ps);
#endif
    if ( *ps == '.' ) ++ps;
    while (*ps)
        {
        while (*ps == ' ') ++ps;
        if (( *ps == '\0' ) || ( *ps == '.' )) break;
#if DEBUG
        dbgmsg ("ps = %s\r\n", ps);
#endif
        const char *ps1 = ps;
        while (true)
            {
            if (*ps == '=')
                {
                int n = ps - ps1;
#if DEBUG
                dbgmsg ("n = %d\r\n", n);
#endif
                iPar = -1;
                for (int i = 0; i < sizeof (psPar) / sizeof (psPar[0]); ++i)
                    {
                    if (( n == strlen (psPar[i]) ) && ( ! strncasecmp (ps1, psPar[i], n) ))
                        {
                        iPar = i;
                        break;
                        }
                    }
#if DEBUG
                dbgmsg ("keyword = %d\r\n", iPar);
#endif
                if ( iPar < 0 ) return false;
                ps1 = ps + 1;
                }
            else if ((*ps == '\0' ) || (*ps == ' ') || (*ps == '.'))
                {
#if DEBUG
                dbgmsg ("parameter = %d\r\n", iPar);
#endif
                if ( iPar == 1 )
                    {
                    if (( *ps1 == 'N' ) || ( *ps1 == 'n' )) sc->parity = UART_PARITY_NONE;
                    else if (( *ps1 == 'E' ) || ( *ps1 == 'e' )) sc->parity = UART_PARITY_EVEN;
                    else if (( *ps1 == 'O' ) || ( *ps1 == 'o' )) sc->parity = UART_PARITY_ODD;
                    else return false;
                    }
                else
                    {
                    int n = 0;
                    while (ps1 < ps)
                        {
                        if ((*ps1 < '0') || (*ps1 > '9')) return false;
                        n = 10 * n + *ps1 - '0';
                        ++ps1;
                        }
                    ((int *)(&sc->baud))[iPar] = n;
#if DEBUG
                    dbgmsg ("iPar = %d, n = %d\r\n", iPar, n);
#endif
                    }
                ++iPar;
                ++ps;
                break;
                }
            ++ps;
            }
        }
    if ( sc->data < 0 ) sc->data = 8;
    if ( sc->stop < 0 ) sc->stop = 1;
    if ( sc->parity < 0 ) sc->parity = UART_PARITY_NONE;
#if DEBUG
    dbgmsg ("sconfig (%d, %d, %d, %d, %d, %d, %d, %d)\r\n", sc->baud, sc->parity, sc->data, sc->stop,
        sc->tx, sc->rx, sc->cts, sc->rts);
#endif
    return true;
    }
#endif
