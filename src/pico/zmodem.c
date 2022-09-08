// zmodem.c - An implementation of the zmodem file transfer protocol

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include "crctab.h"
#include "lfswrap.h"

int anykey (unsigned char *key, int tmo);
void error (int iErr, const char *psMsg);
char *setup (char *dst, const char *src, char *ext, char term, unsigned char *pflag);
#define MAX_PATH    260

#define SOH         0x01
#define EOT         0x04
#define ACK         0x06
#define CR          0x0D
#define DLE         0x10
#define XON         0x11
#define XOFF        0x13
#define NAK         0x15
#define CAN         0x18

#define ZHDR_BIN    0x101       /* Binary header */
#define ZHDR_HEX    0x102       /* Hex header */
#define ZHDR_B32    0x103       /* Binary header with 32-bit CRC */

#define ZCRC_16     ZHDR_BIN    /* 16-bit CRC */
#define ZCRC_32     ZHDR_B32    /* 32-bit CRC */

#define ZHT_RQINIT	    0	/* Request receive init */
#define ZHT_RINIT	    1	/* Receive init */
#define ZHT_SINIT       2	/* Send init sequence (optional) */
#define ZHT_ACK         3	/* ACK to above */
#define ZHT_FILE        4	/* File name from sender */
#define ZHT_SKIP        5	/* To sender: skip this file */
#define ZHT_NAK         6	/* Last packet was garbled */
#define ZHT_ABORT       7	/* Abort batch transfers */
#define ZHT_FIN         8	/* Finish session */
#define ZHT_RPOS        9	/* Resume data trans at this position */
#define ZHT_DATA        10	/* Data packet(s) follow */
#define ZHT_EOF         11	/* End of file */
#define ZHT_FERR        12	/* Fatal Read or Write error Detected */
#define ZHT_CRC         13	/* Request for file CRC and response */
#define ZHT_CHALLENGE   14	/* Receiver's Challenge */
#define ZHT_COMPL       15	/* Request is complete */
#define ZHT_CAN         16	/* Other end canned session with CAN*5 */
#define ZHT_FREECNT     17	/* Request for free bytes on filesystem */
#define ZHT_COMMAND     18	/* Command from sending program */
#define ZHT_STDERR      19  /* Output to standard error, data follows */
#define ZST_NEWHDR      20  /* Read a new header */
#define ZST_PEEKHDR     21  /* Check for a new header, continue if none */
#define ZST_QUIT        22  /* Exit */

#define ZFE_CRCE    ( 'h' | 0x100 )	/* 360 CRC next, frame ends, header packet follows */
#define ZFE_CRCG    ( 'i' | 0x100 )	/* 361 CRC next, frame continues nonstop */
#define ZFE_CRCQ    ( 'j' | 0x100 )	/* 362 CRC next, frame continues, ZACK expected */
#define ZFE_CRCW    ( 'k' | 0x100 )	/* 363 CRC next, ZACK expected, end of frame */

#define ZERR_NONE    0      // No error
#define ZERR_BAD    -1      // Invalid char sequence
#define ZERR_TOUT   -2      // Timeout
#define ZERR_ABT    -3      // Abort (CAN * 5)
#define ZERR_CRC    -4      // CRC failure
#define ZERR_ORUN   -5      // Overrun buffer
#define ZERR_FULL   -6      // Storage full (or other write error)
#define ZERR_NSYNC  -7      // File position out of sync
#define ZERR_INFO   -8      // Error generating file information
#define ZERR_UNIMP  -9      // Command not implemented

#define ZFLG_ESCALL 0x40    // Escape all control characters
static unsigned char txflg;
static FILE *pf = NULL;
static int nfpos = 0;

#define DIAG_HW 1
#if DIAG_HW
#include <hardware/uart.h>
#include <hardware/gpio.h>
#endif

void zdiag (const char *psFmt,...)
    {
    static FILE *pfd = NULL;
    char sMsg[128];
    va_list va;
    va_start (va, psFmt);
    vsprintf (sMsg, psFmt, va);
    va_end (va);
#if DIAG_HW
    if ( pfd == NULL )
        {
        uart_init (uart1, 115200);
        uart_set_format (uart1, 8, 1, UART_PARITY_EVEN);
        gpio_set_function (4, GPIO_FUNC_UART);
        gpio_set_function (5, GPIO_FUNC_UART);
        uart_set_hw_flow (uart1, false, false);
        pfd = (FILE *) 1;
        }
    uart_write_blocking (uart1, sMsg, strlen (sMsg));
#else
    if ( pfd == NULL ) pfd = fopen ("/dev/uart.baud=115200 parity=N data=8 stop=1 tx=4 rx=5", "w");
    fwrite ((void *)sMsg, 1, strlen (sMsg), pfd);
#endif
    }

static int zrdchr (int timeout)
    {
    int key = 0;
    unsigned char kb;
    int nCan = 0;
    absolute_time_t twait = make_timeout_time_ms (timeout);
    do
        {
        if ( anykey (&kb, 100 * timeout) )
            {
            key = kb;
            // if ((key >= 0x20) && (key <0x7F)) zdiag ("%c", key);
            // else zdiag (" %02X", key);
            // zdiag (" r%02X", key);
            switch (key)
                {
                case CAN:
                    ++nCan;
                    if ( nCan == 5 ) return ZERR_ABT;
                    break;
                case XON:
                case XOFF:
                    break;
                case 'h':
                case 'i':
                case 'j':
                case 'k':
                    if ( nCan > 0 ) key |= 0x100;
                    return key;
                default:
                    if ( nCan > 0 )
                        {
                        if (( key & 0x60 ) == 0x40 ) return (key ^ 0x40) | 0x100;
                        return ZERR_BAD;
                        }
                    return key;
                }
            }
        }
    while ( get_absolute_time () < twait );
    return ZERR_TOUT;
    }

static bool zpeekchr (int pchr, int tmo)
    {
    int key;
    zdiag ("zpeekchr:");
    do
        {
        key = zrdchr (tmo);
        zdiag (" %02X", key);
        }
    while (( key != pchr ) && ( key != ZERR_TOUT ));
    zdiag ("\r\n");
    return ( key == pchr );
    }

static inline void zcrcini (int type, long *pcrc)
    {
    if ( type == ZHDR_B32 )
        {
        *pcrc = 0xFFFFFFFFL;
        }
    else
        {
        *pcrc = 0;
        }
    }

static inline void zcrcupd (int type, long *pcrc, unsigned char b)
    {
    if ( type == ZHDR_B32 )
        {
        *pcrc = UPDC32 (b, *pcrc);
        }
    else
        {
        *pcrc = updcrc (b, *pcrc);
        }
    }

static int zchkhdr (int type, const unsigned char *hdr)
    {
    long chk;
    zcrcini (type, &chk);
    for (int i = 0; i < 5; ++i) zcrcupd (type, &chk, hdr[i]);
    if ( type == ZHDR_B32 )
        {
        for (int i = 5; i < 9; ++i)
            {
            if ( hdr[i] != (chk & 0xFF) ) return ZERR_CRC;
            chk >>= 8;
            }
        }
    else
        {
        zcrcupd (type, &chk, 0);
        zcrcupd (type, &chk, 0);
        if (( hdr[5] != (chk >> 8) ) || ( hdr[6] != (chk & 0xFF) )) return ZERR_CRC;
        }
    return hdr[0];
    }

static int zrdhdr (unsigned char *hdr)
    {
    int type;
    int key;
    bool bHavePad = false;
    while (true)
        {
        type = zrdchr (60000);
        if ( type < ZERR_BAD ) return type;
        if ( bHavePad && ( type >= ZHDR_BIN ) && ( type <= ZHDR_B32 )) break;
        else if ( type == '*' ) bHavePad = true;
        else bHavePad = false;
        }
    int nhdr = 7;
    if ( type == ZHDR_HEX )
        {
        for (int i = 0; i < nhdr; ++i)
            {
            key = zrdchr (1000);
            if ( key < 0 ) return key;
            int hx1 = key;
            if (( hx1 >= '0' ) && ( hx1 <= '9' ))   hx1 -= '0';
            else if (( hx1 >= 'a' ) && ( hx1 <= 'f' )) hx1 -= 'a' - 10;
            else ZERR_BAD;
            key = zrdchr (1000);
            if ( key < 0 ) return key;
            if (( key >= '0' ) && ( key <= '9' ))   key -= '0';
            else if (( key >= 'a' ) && ( key <= 'f' )) key -= 'a' - 10;
            else ZERR_BAD;
            hdr[i] = ( hx1 << 4 ) | key;
            }
        }
    else
        {
        if ( type == ZHDR_B32 ) nhdr = 9;
        for (int i = 0; i < nhdr; ++i)
            {
            key = zrdchr (1000);
            if ( key < 0 ) return key;
            hdr[i] = key & 0xFF;
            }
        }
    return zchkhdr (type, hdr);
    }

static int ztxthdr (const char *ps, unsigned char *hdr)
    {
    int hx1;
    int hx2;
    for (int i = 0; i < 7; ++i)
        {
        hx1 = *ps;
        if (( hx1 >= '0' ) && ( hx1 <= '9' )) hx1 -= '0';
        else if (( hx1 >= 'a' ) && ( hx1 <= 'f' )) hx1 -= 'a' - 10;
        else return ZERR_BAD;
        ++ps;
        hx2 = *ps;
        if (( hx2 >= '0' ) && ( hx2 <= '9' )) hx2 -= '0';
        else if (( hx2 >= 'a' ) && ( hx2 <= 'f' )) hx2 -= 'a' - 10;
        else return ZERR_BAD;
        ++ps;
        hdr[i] = ( hx1 << 4 ) | hx2;
        }
    return zchkhdr (ZHDR_HEX, hdr);
    }

static void zwrchr (int chr)
    {
    static bool bAt = false;
    if ( chr >= 0x100 )
        {
        // zdiag (" CAN");
        putchar_raw (CAN);
        chr = ( chr & 0xFF ) | 0x40;
        }
    else if ( (chr & 0x7F) < 0x20 )
        {
        if ( txflg & ZFLG_ESCALL )
            {
            // zdiag (" CAN");
            putchar_raw (CAN);
            chr |= 0x40;
            }
        else
            {
            switch (chr & 0x7F)
                {
                case DLE:
                case XON:
                case XOFF:
                case CAN:
                    // zdiag (" CAN");
                    putchar_raw (CAN);
                    chr |= 0x40;
                    break;
                case CR:
                    if ( bAt )
                        {
                        // zdiag (" CAN");
                        putchar_raw (CAN);
                        chr |= 0x40;
                        }
                    break;
                default:
                    break;
                }
            }
        }
    // zdiag (" w%02x", chr);
    putchar_raw (chr);
    bAt = (chr == '@');
    }

static void zwrhex (int chr)
    {
    int hx = chr >> 4;
    if ( hx < 10 ) hx += '0';
    else hx += 'a' - 10;
    zwrchr (hx);
    hx = chr & 0x0F;
    if ( hx < 10 ) hx += '0';
    else hx += 'a' - 10;
    zwrchr (hx);
    }

static void zwrhdr (int type, unsigned char *hdr)
    {
    zwrchr ('*');
    if ( type == ZHDR_HEX ) zwrchr ('*');
    zwrchr (type);
    long chk;
    zcrcini (type, &chk);
    zdiag ("zwrhdr:");
    for (int i = 0; i < 5; ++i)
        {
        if ( type == ZHDR_HEX )
            {
            zwrhex (hdr[i]);
            }
        else
            {
            zwrchr (hdr[i]);
            }
        zcrcupd (type, &chk, hdr[i]);
        zdiag (" %02X=%04X", hdr[i], chk);
        }
    if ( type == ZHDR_BIN )
        {
        long test = chk;
        zcrcupd (type, &chk, 0);
        zdiag (" 0=%04X", chk);
        zcrcupd (type, &chk, 0);
        zdiag (" 0=%04X", chk);
        zwrchr (chk >> 8);
        zcrcupd (type, &test, chk >> 8);
        zdiag (" c=%04X", test);
        zwrchr (chk & 0xFF);
        zcrcupd (type, &test, chk & 0xFF);
        zdiag (" c=%04X\r\n", test);
        }
    else if ( type == ZHDR_HEX )
        {
        long test = chk;
        zcrcupd (type, &chk, 0);
        zdiag (" 0=%04X", chk);
        zcrcupd (type, &chk, 0);
        zdiag (" 0=%04X", chk);
        zwrhex (chk >> 8);
        zcrcupd (type, &test, chk >> 8);
        zdiag (" c=%04X", test);
        zwrhex (chk & 0xFF);
        zcrcupd (type, &test, chk & 0xFF);
        zdiag (" c=%04X\r\n", test);
        zwrchr (0x0D);
        zwrchr (0x0A);
        long crc = chk;
        }
    if ( type == ZHDR_B32 )
        {
        for (int i = 0; i < 4; ++i)
            {
            zwrchr (chk & 0xFF);
            chk >>= 8;
            }
        }
    }

static int zchkcrc (long chk, int state)
    {
    zcrcupd (ZCRC_16, &chk, 0);
    zcrcupd (ZCRC_16, &chk, 0);
    int crc1 = zrdchr (1000);
    if ( crc1 < 0 ) return crc1;
    crc1 &= 0xFF;
    int crc2 = zrdchr (1000);
    if ( crc2 < 0 ) return crc2;
    crc2 &= 0xFF;
    if ((( chk >> 8 ) != crc1 ) || (( chk & 0xFF ) != crc2 )) return ZERR_CRC;
    return state;
    }

static int zrddata (void *ptr, int nlen)
    {
    unsigned char *buffer;
    int nalloc = 0;
    int ndata = 0;
    long chk = 0;
    int key;
    if ( nlen < 0 )
        {
        nlen = -nlen;
        nalloc = nlen;
        buffer = (unsigned char *) malloc(nlen);
        *((unsigned char **)ptr) = buffer;
        if ( buffer == NULL ) return ZERR_ORUN;
        }
    else
        {
        buffer = (unsigned char *)ptr;
        }
    zcrcini (ZCRC_16, &chk);
    while (true)
        {
        key = zrdchr (1000);
        if ( key < 0 ) return key;
        zcrcupd (ZCRC_16, &chk, (unsigned char) key);
        if ( key >= ZFE_CRCE ) break;
        key &= 0xFF;
        if (( ndata >= nlen ) && ( nalloc > 0 ))
            {
            unsigned char *bnew = realloc (buffer, ndata + nalloc);
            if ( bnew != NULL )
                {
                buffer = bnew;
                ndata += nalloc;
                *((unsigned char **)ptr) = buffer;
                }
            else
                {
                nalloc = -nalloc;
                }
            }
        if ( ndata < nlen ) buffer[ndata] = key;
        ++ndata;
        }
    if ( ndata > nlen ) return ZERR_ORUN;
    return zchkcrc (chk, key);
    }

static FILE *pathopen (char *path, const char *pfname)
    {
    char *ps = strrchr (path, '/');
    if ( ps == NULL ) ps = path;
    else ++ps;
    int nch = strlen (pfname) + (ps - path);
    if ( nch > MAX_PATH ) return NULL;
    strcpy (ps, pfname);
    return fopen (path, "w");
    }

static int zrdfinfo (unsigned char *hdr, char *path)
    {
    unsigned char *pinfo = NULL;
    int state = zrddata (&pinfo, -64);
    zdiag ("\r\nFile = %s\r\n", pinfo);
    if ( state < 0 )
        {
        if ( pinfo != NULL ) free (pinfo);
        return state;
        }
    if ( pf == NULL )
        {
        pf = pathopen (path, pinfo);
        if ( pf == NULL )
            {
            free (pinfo);
            return ZHT_FERR;
            }
        }
    nfpos = 0;
    free (pinfo);
    return ZERR_NSYNC; // Force a ZHT_POS response
    }

static inline int hdrint (unsigned char *hdr)
    {
    return ( hdr[1] | ( hdr[2] << 8 ) | ( hdr[3] << 16 ) | ( hdr[4] << 24 ));
    }

static void zsethdr (unsigned char *hdr, int ht, int val)
    {
    hdr[0] = ht;
    hdr[1] = val & 0xFF;
    val >>= 8;
    hdr[2] = val & 0xFF;
    val >>= 8;
    hdr[3] = val & 0xFF;
    val >>= 8;
    hdr[4] = val;
    }

static bool zsvbuff (int npos, int ndata, const unsigned char *data)
    {
    int nfst = nfpos - npos;
    int nwrt = ndata - nfst;
    zdiag ("npos = %d ndata = %d nfst = %d nwrt = %d", npos, ndata, nfst, nwrt);
    if ( nwrt > 0 )
        {
        int nsave = fwrite ((void *)&data[nfst], 1, nwrt, pf);
        nfpos += nsave;
        zdiag (" nwrt = %d, nsave = %d, nfpos = %d", nwrt, nsave, nfpos);
        if ( nsave < ndata ) return false;
        }
    zdiag ("\r\n");
    return true;
    }

static int zsvdata (unsigned char *hdr)
    {
    long chk;
    int key;
    unsigned char data[64];
    int ndata = 0;
    int npos = hdrint (hdr);
    zdiag ("zsvdata: nfpos = %d, hdr = %d\r\n", nfpos, npos);
    if ( npos > nfpos ) return ZERR_NSYNC;
    zcrcini (ZCRC_16, &chk);
    while (true)
        {
        key = zrdchr (1000);
        if ( key < 0 ) return key;
        zcrcupd (ZCRC_16, &chk, key);
        if ( key >= ZFE_CRCE ) break;
        key &= 0xFF;
        data[ndata] = key;
        if ( ++ndata >= sizeof (data) )
            {
            if ( ! zsvbuff (npos, ndata, data) ) return ZERR_FULL;
            npos += ndata;
            ndata = 0;
            }
        }
    if ( ndata > 0 )
        {
        if ( ! zsvbuff (npos, ndata, data) ) return ZERR_FULL;
        npos += ndata;
        ndata = 0;
        }
    zsethdr (hdr, hdr[0], npos);    // Record new position for continuing data
    return zchkcrc (chk, key);
    }

static void zwrdata (int type, const unsigned char *pdata, int ndata, int fend)
    {
    long chk;
    zcrcini (type, &chk);
    zdiag ("zwrdata:");
    for (int i = 0; i < ndata; ++i)
        {
        zwrchr (*pdata);
        zcrcupd (type, &chk, *pdata);
        zdiag (" %02X=%04X", *pdata, chk);
        ++pdata;
        }
    zwrchr (fend);
    zcrcupd (type, &chk, fend & 0xFF);
    zdiag (" %02X=%04X", fend & 0xFF, chk);
    long test = chk;
    zcrcupd (type, &chk, 0);
    zdiag (" 0=%04X", chk);
    zcrcupd (type, &chk, 0);
    zdiag (" 0=%04X", chk);
    zwrchr (chk >> 8);
    zcrcupd (type, &test, chk >> 8);
    zdiag (" c=%04X", test);
    zwrchr (chk & 0xFF);
    zcrcupd (type, &test, chk & 0xFF);
    zdiag (" c=%04X\r\n", test);
    }

static int zwrfinfo (int type, const char *pfn)
    {
    zdiag ("pfn = %s pf = %p\r\n", pfn, pf);
    int nch = strlen (pfn);
    unsigned char *pinfo = (unsigned char *) malloc (nch + 12);
    if ( pinfo == NULL ) return ZERR_INFO;
    strcpy (pinfo, pfn);
    zdiag ("Seek to file end\r\n");
    if ( fseek (pf, 0, SEEK_END) != 0 ) return ZERR_INFO;
    long nlen = ftell (pf);
    zdiag ("nlen = %d\r\n", nlen);
    if ( nlen < 0 ) return ZERR_INFO;
    if ( fseek (pf, 0, SEEK_SET) != 0 ) return ZERR_INFO;
    zdiag ("Rewound file\r\n");
    nfpos = 0;
    sprintf (&pinfo[nch+1], "%d", nlen);
    nch += strlen (&pinfo[nch+1]) + 2;
    zwrdata (type, pinfo, nch, ZFE_CRCW);
    free (pinfo);
    return ZST_NEWHDR;
    }

static int zwrfile (int type, unsigned char *hdr)
    {
    int state;
    int fend;
    unsigned char data[64];
    int npos = hdrint (hdr);
    zdiag ("zwrfile: npos = %d nfpos = %d\r\n", npos, nfpos);
    if ( npos != nfpos )
        {
        if ( fseek (pf, npos, SEEK_SET) < 0 )
            {
            return ZHT_FERR;
            }
        nfpos = npos;
        }
    int ndata = fread (data, 1, sizeof (data), pf);
    nfpos += ndata;
    zsethdr (hdr, hdr[0], nfpos);
    if ( ndata == sizeof (data) )
        {
        state = ZST_PEEKHDR;
        fend = ZFE_CRCG;
        // state = ZST_NEWHDR;
        // fend = ZFE_CRCQ;
        }
    else
        {
        zdiag ("zwrfile: ndata = %d nfpos = %d\r\n", ndata, nfpos);
        state = ZHT_EOF;
        fend = ZFE_CRCE;
        }
    zwrdata (type, data, ndata, fend);
    return state;
    }

static int zwrbreak (unsigned char *hdr, unsigned char *attn)
    {
    unsigned char *pa = attn;
    while ( *pa )
        {
        switch (*pa)
            {
            case 0xDD:
                // Should be a break
                sleep_ms (1000);
                break;
            case 0xDE:
                // A pause
                sleep_ms (1000);
                break;
            default:
                zdiag (" w%02x", *pa);
                putchar_raw (*pa);
                break;
            }
        ++pa;
        }
    zwrhdr (ZHDR_HEX, hdr);
    return ZST_NEWHDR;
    }

static void zclose (void)
    {
    zdiag ("zclose\r\n");
    if ( pf != NULL ) fclose (pf);
    pf = NULL;
    }

void zreceive (const char *pfname, const char *pcmd)
    {
    unsigned char attn[32] = { 0 };
	char path[MAX_PATH+1];
	unsigned char flag;
    zdiag ("zreceive (%s, %s)\r\n", pfname, pcmd);
    setup (path, pfname, ".bbc", ' ', &flag);
    zdiag ("path = %s flag = %02X\r\n", path, flag);
    if ( flag & 0x01 )
        {
        pf = fopen (path, "w");
        if ( pf == NULL ) error (204, "Cannot create file");
        }
    int state = ZHT_RQINIT;
    int curst;
    int ntmo = 10;
    int nnak = 10;
    unsigned char hdr[9];
    if ( pcmd != NULL )
        {
        pcmd += 2;
        state = ztxthdr (pcmd, hdr);
        }
    do
        {
        zdiag ("state = %d\r\n", state);
        if (( state >= 0 ) && ( state < ZST_QUIT )) curst = state;
        switch (state)
            {
            case ZST_NEWHDR:
                state = zrdhdr (hdr);
                break;
            case ZHT_RQINIT:    /* 0 - Request receive init */
                zsethdr (hdr, ZHT_RINIT, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_SINIT:     /* 2 - Send init sequence (optional) */
                txflg = hdr[4];
                state = zrddata (attn, sizeof (attn));
                break;
            case ZHT_FILE:      /* 4 - File name from sender */
                state = zrdfinfo (hdr, path);
                break;
            case ZHT_SKIP:      /* 5 - To sender: skip this file */
                state = ZERR_UNIMP;
                break;
            case ZHT_NAK:       /* 6 - Last packet was garbled */
                if ( --nnak == 0 ) state = ZST_QUIT;
                else state = curst;
                break;
            case ZHT_FIN:       /* 8 - Finish session */
                zwrhdr (ZHDR_HEX, hdr);
                zdiag ("ZHT_FIN: ");
                if ( zpeekchr ('O', 1000) )
                    {
                    zpeekchr ('O', 1000);
                    }
                state = ZST_QUIT;
                break;
            case ZHT_DATA:      /* 10 - Data packet(s) follow */
                state = zsvdata (hdr);
                break;
            case ZHT_EOF:       /* 11 - End of file */
                zdiag ("EOF: nfpos = %d, hdr = %d\r\n", nfpos, hdrint (hdr));
                if ( hdrint (hdr) != nfpos ) state = ZERR_NSYNC;
                if ( pf != NULL )
                    {
                    fclose (pf);
                    pf = NULL;
                    }
                nfpos = 0;
                state = ZHT_RQINIT;
                break;
            case ZHT_FERR:      /* 12 - Fatal Read or Write error Detected */
                state = ZHT_RQINIT;
                break;
            case ZHT_RINIT:     /* 1 - Receive init - Echoed from dead client */
            case ZHT_CAN:       /* 16 - Other end canned session with CAN*5 */
                state = ZST_QUIT;
                break;
            case ZHT_FREECNT:   /* 17 - Request for free bytes on filesystem */
                zsethdr (hdr, ZHT_ACK, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_ACK:       /* 3 - ACK to above */
            case ZHT_ABORT:     /* 7 - Abort batch transfers */
            case ZHT_RPOS:      /* 9 - Resume data trans at this position */
            case ZHT_CRC:       /* 13 - Request for file CRC and response */
            case ZHT_CHALLENGE: /* 14 - Receiver's Challenge */
            case ZHT_COMPL:     /* 15 - Request is complete */
            case ZHT_COMMAND:   /* 18 - Command from sending program */
            case ZHT_STDERR:    /* 19 - Output to standard error, data follows */
                state = ZERR_UNIMP;
                break;
            case ZFE_CRCE:      /* CRC next, frame ends, header packet follows */
                state = ZST_NEWHDR;
                break;
            case ZFE_CRCG:       /* CRC next, frame continues nonstop */
                state = curst;
                break;
            case ZFE_CRCQ:      /* CRC next, frame continues, ZACK expected */
                zsethdr (hdr, ZHT_ACK, nfpos);
                zwrhdr (ZHDR_HEX, hdr);
                state = curst;
                break;
            case ZFE_CRCW:      /* CRC next, ZACK expected, end of frame */
                zsethdr (hdr, ZHT_ACK, nfpos);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZERR_TOUT:
                zdiag ("ntmo = %d\r\n", ntmo);
                if ( --ntmo == 10 )
                    {
                    state = ZST_QUIT;
                    }
                else
                    {
                    zsethdr (hdr, ZHT_NAK, 0);
                    zwrhdr (ZHDR_HEX, hdr);
                    state = ZST_NEWHDR;
                    }
                break;
            case ZERR_ABT:
                state = ZST_QUIT;
                break;
            case ZERR_FULL:
                zsethdr (hdr, ZHT_FERR, nfpos);
                state = zwrbreak (hdr, attn);
                break;
            case ZERR_NSYNC:
                zsethdr (hdr, ZHT_RPOS, nfpos);
                state = zwrbreak (hdr, attn);
                break;
            default:
                zsethdr (hdr, ZHT_NAK, nfpos);
                state = zwrbreak (hdr, attn);
                break;
            }
        }
    while ( state != ZST_QUIT );
    zclose ();
    }

void zsend (const char *pfname)
    {
	char path[MAX_PATH+1];
    bool bDone = false;
    int state = ZHT_RQINIT;
    int curst;
    int prvst = 0;
    int ntmo = 10;
    int nnak = 10;
    unsigned char hdr[9];
	unsigned char flag;
    zdiag ("zsend\r\n");
    setup (path, pfname, ".bbc", ' ', &flag);
    pf = fopen (path, "r");
    if ( pf == NULL ) error (214, "Cannot open file");
    state = ZHT_RQINIT;
    do
        {
        zdiag ("state = %d\r\n", state);
        prvst = curst;
        if (( state >= 0 ) && ( state < ZST_QUIT )) curst = state;
        switch (state)
            {
            case ZST_PEEKHDR:
                if ( zpeekchr ('*', 0) ) state = ZST_NEWHDR;
                else state = ZHT_ACK;
                break;
            case ZST_NEWHDR:
                state = zrdhdr (hdr);
                break;
            case ZHT_RQINIT:    /* 0 - Request receive init */
                zsethdr (hdr, ZHT_RQINIT, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_RINIT:     /* 1 - Receive init */
                zdiag ("ZHT_RINIT: %02X %02X %02X %02X\r\n", hdr[1], hdr[2], hdr[3], hdr[4]);
                if ( bDone )
                    {
                    zsethdr (hdr, ZHT_FIN, 0);
                    zwrhdr (ZHDR_HEX, hdr);
                    state = ZST_NEWHDR;
                    }
                else
                    {
                    zsethdr (hdr, ZHT_FILE, 0);
                    zwrhdr (ZHDR_BIN, hdr);
                    state = zwrfinfo (ZHDR_BIN, path);
                    }
                break;
            case ZHT_ACK:       /* 3 - ACK to above */
                zdiag ("ZHT_ACK: hdr = %d nfpos = %d\r\n", hdrint (hdr), nfpos);
                // zsethdr (hdr, ZHT_DATA, nfpos);
                // zwrhdr (ZHDR_BIN, hdr);
                state = zwrfile (ZHDR_BIN, hdr);
                break;
            case ZHT_SKIP:      /* 5 - To sender: skip this file */
                zsethdr (hdr, ZHT_FIN, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_NAK:       /* 6 - Last packet was garbled */
                if ( --nnak == 0 )
                    {
                    zclose ();
                    return;
                    }
                curst = prvst;
                state = prvst;
                break;
            case ZHT_FIN:       /* 8 - Finish session */
                zwrchr ('O');
                zwrchr ('O');
                state = ZST_QUIT;
                break;
            case ZHT_RPOS:      /* 9 - Resume data trans at this position */
                hdr[0] = ZHT_DATA;
                zwrhdr (ZHDR_BIN, hdr);
                state = zwrfile (ZHDR_BIN, hdr);
                break;
            case ZHT_EOF:       /* 11 - End of file */
                zsethdr (hdr, ZHT_EOF, nfpos);
                zwrhdr (ZHDR_HEX, hdr);
                bDone = true;
                state = ZST_NEWHDR;
                break;
            case ZHT_FERR:      /* 12 - Fatal Read or Write error Detected */
                zsethdr (hdr, ZHT_FIN, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_CAN:       /* 16 - Other end canned session with CAN*5 */
                zdiag ("Cancelled by remote\r\n");
                zclose ();
                return;
            case ZHT_FREECNT:   /* 17 - Request for free bytes on filesystem */
                zsethdr (hdr, ZHT_ACK, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_SINIT:     /* 2 - Send init sequence (optional) */
            case ZHT_FILE:      /* 4 - File name from sender */
            case ZHT_ABORT:     /* 7 - Abort batch transfers */
            case ZHT_DATA:      /* 10 - Data packet(s) follow */
            case ZHT_CRC:       /* 13 - Request for file CRC and response */
            case ZHT_CHALLENGE: /* 14 - Receiver's Challenge */
            case ZHT_COMPL:     /* 15 - Request is complete */
            case ZHT_COMMAND:   /* 18 - Command from sending program */
            case ZHT_STDERR:    /* 19 - Output to standard error, data follows */
                state = ZERR_UNIMP;
                break;
            case ZERR_ABT:
                state = ZST_QUIT;
                break;
            case ZERR_TOUT:
                zdiag ("ntmo = %d\r\n", ntmo);
                if ( --ntmo == 10 ) state = ZST_QUIT;
                else state = curst;
                break;
            case ZERR_INFO:
                zsethdr (hdr, ZHT_FERR, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            default:
                zsethdr (hdr, ZHT_NAK, nfpos);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            }
        }
    while ( state != ZST_QUIT );
    zclose ();
    }

#define ydiag zdiag

static int ychecksum (unsigned char *buffer)
    {
    int csum = 0;
    for (int i = 3; i < 131; ++i)
        {
        csum += buffer[i];
        }
    return csum & 0xFF;
    }

static int ycrcval (unsigned char *buffer)
    {
    int crc = 0;
    for (int i = 3; i < 131; ++i)
        {
        crc = updcrc (buffer[i], crc);
        }
    crc = updcrc (0, crc);
    crc = updcrc (0, crc);
    return crc & 0xFFFF;
    }

void yreceive (int mode, const char *pfname)
    {
	char path[MAX_PATH+1];
    unsigned char buffer[132];
    pf = NULL;
    int nb = 0;
    int nlen = -1;
    int ntmo = 10;
    int nblk = 1;
    int lblk = 1;
    unsigned char key;
    bool bHaveByte = true;
	unsigned char flag;
    ydiag ("Entered yreceive\r\n");
    setup (path, pfname, ".bbc", ' ', &flag);
    if ( flag & 0x01 )
        {
        pf = fopen (path, "w");
        if ( pf == NULL ) error (204, "Unable to create file");
        }
    else if ( mode == 1 )
        {
        error (31, "No file name");
        }
    if ( mode == 3 )
        {
        buffer[0] = SOH;
        nb = 1;
        }
    else
        {
        ntmo = 60;
        putchar_raw (NAK);
        nb = 0;
        }
    while (true)
        {
        bHaveByte = true;
        while (( nb < 132 ) && bHaveByte )
            {
            absolute_time_t twait;
            int tmo = 500;
            if ( nb == 0 ) tmo = 2000;
            twait = make_timeout_time_ms (tmo);
            bHaveByte = false;
            while ( get_absolute_time () < twait )
                {
                if ( anykey (&key, 100 * tmo) )
                    {
                    ydiag (" %d=%02X", nb, key);
                    buffer[nb] = key;
                    if (( nb == 0 ) && ( key == EOT ))
                        {
                        if ( pf != NULL )
                            {
                            fclose (pf);
                            pf = NULL;
                            }
                        putchar_raw (ACK);
                        ydiag ("End of upload\r\n");
                        if ( mode == 1 ) return;
                        nblk = -1;
                        lblk = -1;
                        break;
                        }
                    if (( nb > 0 ) || ( key == SOH ))
                        {
                        ++nb;
                        bHaveByte = true;
                        break;
                        }
                    }
                }
            }
        if ( bHaveByte )
            {
            ydiag ("Received block\r\n");
            int csum = ychecksum (buffer);
            ydiag ("0 = %02X 1 = %02X 2 = %02X 131 = %02X csum = %02X\r\n", buffer[0], buffer[1],
                buffer[2], buffer[131], csum);
            if (( buffer[0] == SOH ) && ( buffer[2] == 255 - buffer[1] ) && ( buffer[131] == csum ))
                {
                if ( buffer[1] == nblk )
                    {
                    ydiag ("Next data block\r\n");
                    if ( pf == NULL )
                        {
                        pf = fopen ("xmodem.tmp", "w");
                        nlen = -1;
                        ydiag ("Default file name: xmodem.tmp\r\n");
                        }
                    int nsave = 128;
                    if (( nlen >= 0 ) && ( nlen < nsave )) nsave = nlen;
                    ydiag ("nsave = %d\r\n", nsave);
                    if ( nsave > 0 )
                        {
                        int nout = fwrite (&buffer[3], 1, nsave, pf);
                        if ( nout < nsave )
                            {
                            ydiag ("Only wrote %d - FAIL\r\nExit yreceive\r\n", nout);
                            fclose (pf);
                            pf = NULL;
                            putchar_raw (CAN);
                            putchar_raw (CAN);
                            return;
                            }
                        nlen -= nsave;
                        }
                    ydiag ("Completed block\r\n");
                    putchar_raw (ACK);
                    lblk = nblk;
                    nblk = ( nblk + 1 ) & 0xFF;
                    ntmo = 10;
                    }
                else if ( buffer[1] == lblk )
                    {
                    ydiag ("Repeated data block\r\n");
                    putchar_raw (ACK);
                    }
                else if (( buffer[1] == 0 ) && ( pf == NULL ))
                    {
                    if ( buffer[3] == 0 )
                        {
                        ydiag ("End of uploads\r\n");
                        putchar_raw (ACK);
                        return;
                        }
                    pf = pathopen (path, &buffer[3]);
                    ydiag ("%s\r\n", &buffer[3]);
                    nlen = atoi (buffer + strlen (&buffer[3]) + 4);
                    if (( pf == NULL ) || ( nlen == 0 ))
                        {
                        ydiag ("Failed initial block\r\nExit yreceive\r\n");
                        putchar_raw (CAN);
                        putchar_raw (CAN);
                        return;
                        }
                    ydiag ("Completed initial block\r\n");
                    putchar_raw (ACK);
                    nblk = 1;
                    lblk = 0;
                    ntmo = 10;
                    }
                else
                    {
                    ydiag ("Block out of order\r\n");
                    putchar_raw (NAK);
                    }
                }
            }
        else
            {
            if ( --ntmo == 0 )
                {
                ydiag ("Too many retries\r\nExit yreceive\r\n");
                if ( pf != NULL )
                    {
                    fclose (pf);
                    pf = NULL;
                    }
                putchar_raw (CAN);
                putchar_raw (CAN);
                return;
                }
            ydiag ("Timeout\r\n");
            putchar_raw (NAK);
            }
        nb = 0;
        }
    }

static int yzeroblk (unsigned char *buffer, const char *path)
    {
    const char *ps = strrchr (path, '/');
    if ( ps == NULL ) ps = path;
    else ++ps;
    memset (&buffer[1], 0, 130);
    buffer[2] = 255;
    strncpy (&buffer[3], path, 116);
    if ( fseek (pf, 0, SEEK_END) != 0 ) return -1;
    long nlen = ftell (pf);
    ydiag ("nlen = %d\r\n", nlen);
    if ( fseek (pf, 0, SEEK_SET) != 0 ) return -1;
    sprintf (&buffer[strlen (&buffer[3]) + 4], "%d", nlen);
    buffer[131] = ychecksum (buffer);
    return nlen;
    }

static void ysetcrc (unsigned char *buffer)
    {
    int crc = ycrcval (buffer);
    buffer[131] = crc >> 8;
    buffer[132] = crc & 0xFF;
    }

static int ydatablk (unsigned char *buffer, int nblk, bool bCrc)
    {
    buffer[1] = nblk & 0xFF;
    buffer[2] = 255 - buffer[1];
    memset (&buffer[3], 0, 128);
    int nread = fread (&buffer[3], 1, 128, pf);
    if ( bCrc ) ysetcrc (buffer);
    else buffer[131] = ychecksum (buffer);
    return nread;
    }

static unsigned char ygetchr (int tmo)
    {
    unsigned char key = 0;
    bool bCan = false;
    absolute_time_t twait = make_timeout_time_ms (tmo);
    do
        {
        absolute_time_t t1 = get_absolute_time ();
        if ( anykey (&key, 100 * tmo) )
            {
            absolute_time_t t2 = get_absolute_time ();
            ydiag (" %02X-%dms", key, (int) absolute_time_diff_us (t1, t2) / 1000);
            if (( key == 'C' ) || ( key == ACK ) || ( key == NAK )) return key;
            else if ( key == CAN )
                {
                if ( bCan ) return key;
                else bCan = true;
                }
            else bCan = false;
            }
        }
    while ( get_absolute_time () < twait );
    ydiag (" timeout");
    return 0;
    }

static void ysendblk (unsigned char *buffer, bool bCrc)
    {
    ydiag ("Send block %d\r\n", buffer[1]);
    for (int i = 0; i < 132; ++i)
        {
        putchar_raw (buffer[i]);
        ydiag (" %02X", buffer[i]);
        }
    if ( bCrc )
        {
        putchar_raw (buffer[132]);
        ydiag (" %02X", buffer[132]);
        }
    ydiag ("\r\n");
    }

void ysend (int mode, const char *pfname)
    {
    char path[MAX_PATH+1];
    unsigned char buffer[132];
    unsigned char flag;
    int ntmo = 60;
    int tmo = 4000;
    bool bCrc = false;
    ydiag ("ysend\r\n");
    buffer[0] = SOH;
    setup (path, pfname, ".bbc", ' ', &flag);
    pf = fopen (path, "r");
    if ( pf == NULL ) error (214, "Cannot open file");
    int nblk = 0;
    int ndata;
    if ( mode == 1 )
        {
        nblk = 1;
        ndata = ydatablk (buffer, nblk, bCrc);
        if ( ndata == 0 ) error (255, "Empty file");
        }
    else
        {
        ndata = yzeroblk (buffer, path);
        if ( ndata == 0  ) error (255, "Empty file");
        else if ( ndata < 0 ) error (255, "File seek error");
        }
    while (true)
        {
        unsigned char key = ygetchr (tmo);
        unsigned char key2 = 0;
        if ((nblk == 0 ) && (key == ACK)) key2 = ygetchr (5);
        ydiag ("\r\nnblk = %d:", nblk);
        if (( key == 'C' ) || ( key2 == 'C' ))
            {
            ydiag ("16-bit CRC requested\r\n");
            bCrc = true;
            int crc = ycrcval (buffer);
            buffer[131] = crc >> 8;
            buffer[132] = crc & 0xFF;
            }
        else if (( key == 0 ) || ( key == 'C' ) || ( key == NAK )) --ntmo;
        if (( key == CAN ) || ( ntmo == 0 ))
            {
            ydiag ("Cancelled by remote\r\n");
            fclose (pf);
            return;
            }
        if ( key == ACK )
            {
            ++nblk;
            ndata = ydatablk (buffer, nblk, bCrc);
            ydiag ("Block %d: length = %d\r\n", nblk, ndata);
            if ( ndata == 0 ) break;
            ntmo = 10;
            }
        ysendblk (buffer, bCrc);
        }
    ydiag ("End of file\r\n");
    unsigned char key;
    for (int i = 0; i < 5; ++i)
        {
        ydiag ("EOT:");
        putchar_raw (EOT);
        key = ygetchr (2000);
        if ( key == ACK ) break;
        ydiag ("\r\n");
        }
    if ( key == ACK )
        {
        ydiag ("\r\n");
        memset (&buffer[1], 0, 130);
        buffer[2] = 255;
        buffer[131] = ychecksum (buffer);
        while (true)
            {
            key = ygetchr (2000);
            if ( key == ACK ) break;
            if ( key == 'C' )
                {
                bCrc = true;
                ysetcrc (buffer);
                }
            ysendblk (buffer, bCrc);
            }
        }
    else
        {
        putchar_raw (CAN);
        putchar_raw (CAN);
        }
    ydiag ("\r\nQuit\r\n");
    fclose (pf);
    }
