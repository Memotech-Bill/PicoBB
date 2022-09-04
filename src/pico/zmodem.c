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
#define ZERR_UNIMP  -8      // Command not implemented

#define ZFLG_ESCALL 0x40    // Escape all control characters
static unsigned char txflg;
static unsigned char attn[32] = { 0 };
static FILE *pf = NULL;
static int nfpos = 0;

void zdiag (const char *psFmt,...)
    {
    static FILE *pfd = NULL;
    char sMsg[128];
    va_list va;
    va_start (va, psFmt);
    vsprintf (sMsg, psFmt, va);
    va_end (va);
    if ( pfd == NULL ) pfd = fopen ("/dev/uart.baud=115200 parity=N data=8 stop=1 tx=4 rx=5", "w");
    fwrite ((void *)sMsg, 1, strlen (sMsg), pfd);
    }

static int zrdchr (int timeout)
    {
    int key = 0;
    unsigned char kb;
    int nCan = 0;
    absolute_time_t twait = make_timeout_time_ms (timeout);
    while ( get_absolute_time () < twait )
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
    return ZERR_TOUT;
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
        }
    if ( type == ZHDR_BIN )
        {
        zcrcupd (type, &chk, 0);
        zcrcupd (type, &chk, 0);
        zwrchr (chk >> 8);
        zwrchr (chk & 0xFF);
        }
    else if ( type == ZHDR_HEX )
        {
        zcrcupd (type, &chk, 0);
        zcrcupd (type, &chk, 0);
        zwrhex (chk >> 8);
        zwrhex (chk & 0xFF);
        zwrchr (0x0D);
        zwrchr (0x0A);
        long crc = chk;
        zcrcupd (type, &crc, chk >> 8);
        zcrcupd (type, &crc, chk & 0xFF);
        // zdiag (" crcchk = %04X", crc);
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

static int zrdfinfo (unsigned char *hdr)
    {
    unsigned char *pinfo;
    int state = zrddata (&pinfo, -64);
    if ( state < 0 ) return state;
    if ( pf != NULL ) fclose (pf);
    zdiag ("\r\nFile = %s\r\n", pinfo);
    pf = fopen (pinfo, "w");
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

static void zclose (void)
    {
    zdiag ("zclose\r\n");
    if ( pf != NULL ) fclose (pf);
    pf = NULL;
    }

void zmodem (const char *phex)
    {
    zdiag ("zmodem\r\n");
    int state = ZHT_RQINIT;
    int curst;
    int ntmo = 0;
    unsigned char hdr[9];
    if ( phex != NULL )
        {
        phex += 2;
        }
    while (true)
        {
        zdiag ("Top state = %d\r\n", state);
        if ( phex ) state = ztxthdr (phex, hdr);
        else if ( state == ZST_NEWHDR ) state = zrdhdr (hdr);
        curst = state;
        phex = NULL;
        zdiag ("Header state = %d\r\n", state);
        switch (state)
            {
            case ZHT_RQINIT:    /* 0 - Request receive init */
                zsethdr (hdr, ZHT_RINIT, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_RINIT:     /* 1 - Receive init */
                zclose ();
                return;
                break;
            case ZHT_SINIT:     /* 2 - Send init sequence (optional) */
                txflg = hdr[4];
                state = zrddata (attn, sizeof (attn));
                break;
            case ZHT_ACK:       /* 3 - ACK to above */
                state = ZERR_UNIMP;
                break;
            case ZHT_FILE:      /* 4 - File name from sender */
                state = zrdfinfo (hdr);
                break;
            case ZHT_SKIP:      /* 5 - To sender: skip this file */
                state = ZERR_UNIMP;
                break;
            case ZHT_NAK:       /* 6 - Last packet was garbled */
                zclose ();
                return;
                // state = ZERR_UNIMP;
                break;
            case ZHT_ABORT:     /* 7 - Abort batch transfers */
                state = ZERR_UNIMP;
                break;
            case ZHT_FIN:       /* 8 - Finish session */
                zwrhdr (ZHDR_HEX, hdr);
                zclose ();
                return;
                break;
            case ZHT_RPOS:      /* 9 - Resume data trans at this position */
                state = ZERR_UNIMP;
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
                state = ZERR_UNIMP;
                break;
            case ZHT_CRC:       /* 13 - Request for file CRC and response */
                state = ZERR_UNIMP;
                break;
            case ZHT_CHALLENGE: /* 14 - Receiver's Challenge */
                state = ZERR_UNIMP;
                break;
            case ZHT_COMPL:     /* 15 - Request is complete */
                state = ZERR_UNIMP;
                break;
            case ZHT_CAN:       /* 16 - Other end canned session with CAN*5 */
                state = ZERR_UNIMP;
                break;
            case ZHT_FREECNT:   /* 17 - Request for free bytes on filesystem */
                zsethdr (hdr, ZHT_ACK, 0);
                zwrhdr (ZHDR_HEX, hdr);
                state = ZST_NEWHDR;
                break;
            case ZHT_COMMAND:   /* 18 - Command from sending program */
                state = ZERR_UNIMP;
                break;
            case ZHT_STDERR:    /* 19 - Output to standard error, data follows */
                state = ZERR_UNIMP;
                break;
            default:
                break;
            }
        zdiag ("Exit state = %d\r\n", state);
        if ( state == ZFE_CRCE )
            {
            /* CRC next, frame ends, header packet follows */
            state = ZST_NEWHDR;
            }
        else if ( state == ZFE_CRCG )
            {
            /* CRC next, frame continues nonstop */
            state = curst;
            }
        else if ( state == ZFE_CRCQ )
            {
            /* CRC next, frame continues, ZACK expected */
            zsethdr (hdr, ZHT_ACK, nfpos);
            zwrhdr (ZHDR_HEX, hdr);
            state = curst;
            }
        else if ( state == ZFE_CRCW )
            {
            /* CRC next, ZACK expected, end of frame */
            zsethdr (hdr, ZHT_ACK, nfpos);
            zwrhdr (ZHDR_HEX, hdr);
            state = ZST_NEWHDR;
            }
        else if ( state < 0 )
            {
            if ( state == ZERR_TOUT )
                {
                zdiag ("ntmo = %d\r\n", ntmo);
                if ( ++ntmo >= 10 )
                    {
                    zclose ();
                    return;
                    }
                }
            else
                {
                ntmo = 0;
                }
            switch (state)
                {
                case ZERR_FULL:
                    zsethdr (hdr, ZHT_FERR, nfpos);
                    break;
                case ZERR_NSYNC:
                    zsethdr (hdr, ZHT_RPOS, nfpos);
                    break;
                default:
                    zsethdr (hdr, ZHT_NAK, nfpos);
                    break;
                }
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
            state = ZST_NEWHDR;
            }
        }
    }

#define ydiag zdiag

void ymodem (int mode)
    {
    unsigned char buffer[132];
    FILE *fp = NULL;
    int nb = 0;
    int nlen = 0;
    int nrtry = 0;
    int nblk = 1;
    int lblk = 1;
    unsigned char key;
    bool bHaveByte = true;
    ydiag ("Entered ymodem\r\n");
    if ( mode == 1 )
        {
        buffer[0] = SOH;
        nb = 1;
        }
    else
        {
        putchar_raw (NAK);
        nb = 0;
        }
    while (true)
        {
        bHaveByte = true;
        while (( nb < 132 ) && bHaveByte )
            {
            absolute_time_t twait;
            int tmo = 1000;
            if ( nb == 0 ) tmo = 10000;
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
                        if ( fp != NULL )
                            {
                            fclose (fp);
                            fp = NULL;
                            }
                        putchar_raw (ACK);
                        ydiag ("End of upload\r\nExit ymodem\r\n");
                        return;
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
            int csum = 0;
            for (int i = 3; i < 131; ++i)
                {
                csum += buffer[i];
                }
            csum &= 0xFF;
            ydiag ("0 = %02X 1 = %02X 2 = %02X 131 = %02X csum = %02X\r\n", buffer[0], buffer[1],
                buffer[2], buffer[131], csum);
            if (( buffer[0] == SOH ) && ( buffer[2] == 255 - buffer[1] ) && ( buffer[131] == csum ))
                {
                if ( buffer[1] == nblk )
                    {
                    ydiag ("Next data block\r\n");
                    if ( fp == NULL )
                        {
                        fp = fopen ("xmodem.tmp", "w");
                        nlen = -1;
                        ydiag ("Default file name: xmodem.tmp\r\n");
                        }
                    int nsave = 128;
                    if (( nlen >= 0 ) && ( nlen < nsave )) nsave = nlen;
                    ydiag ("nsave = %d\r\n", nsave);
                    if ( nsave > 0 )
                        {
                        int nout = fwrite (&buffer[3], 1, nsave, fp);
                        if ( nout < nsave )
                            {
                            ydiag ("Only wrote %d - FAIL\r\nExit ymodem\r\n", nout);
                            fclose (fp);
                            fp = NULL;
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
                    nrtry = 0;
                    }
                else if ( buffer[1] == lblk )
                    {
                    ydiag ("Repeated data block\r\n");
                    putchar_raw (ACK);
                    }
                else if (( buffer[1] == 0 ) && ( fp == NULL ))
                    {
                    fp = fopen (&buffer[3], "wb");
                    ydiag ("%s\r\n", &buffer[3]);
                    unsigned char *p = buffer + strlen (&buffer[3]) + 4;
                    nlen = 0;
                    while ((*p >= '0') && (*p <= '9'))
                        {
                        nlen = 10 * nlen + *p - '0';
                        ++p;
                        }
                    if (( fp == NULL ) || ( nlen == 0 ))
                        {
                        ydiag ("Failed initial block\r\nExit ymodem\r\n");
                        putchar_raw (CAN);
                        putchar_raw (CAN);
                        return;
                        }
                    ydiag ("Completed initial block\r\n");
                    nblk = 1;
                    lblk = 1;
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
            if ( ++nrtry >= 10 )
                {
                ydiag ("Too many retries\r\nExit ymodem\r\n");
                if ( fp != NULL )
                    {
                    fclose (fp);
                    fp = NULL;
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
