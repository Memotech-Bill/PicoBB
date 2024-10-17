/*  uf2merge.c - Merge multiple UF2 files into a single file

Usage: uf2merge output.uf2 input1.uf2 input2.uf2

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>

#define BLK_SIZE    512
#define BLK_END     ( BLK_SIZE / 4 - 1 )
#define MAGIC_0     0x0A324655
#define MAGIC_1     0x9E5D5157
#define MAGIC_2     0x0AB16F30
#define FLG_FAMILY  0x00002000
#define SECTOR_SIZE 4096
#define SECTOR_MASK (~(SECTOR_SIZE - 1))

typedef struct
    {
    int         iArg;
    uint32_t    nBlocks;
    uint32_t    iAddrMin;
    uint32_t    iAddrMax;
    uint32_t    iAddrBegin;
    uint32_t    iAddrEnd;
    } FILEINFO;

void help (const char *psProg)
    {
    fprintf (stderr, "Usage: %s [-h] [-e] [-f | -i] [-p pad] -o output.uf2 input1.uf2 input2.uf2...\n", psProg);
    fprintf (stderr, "       -h - Display this help and exit\n");
    fprintf (stderr, "       -e - Erase (fill) gaps between inputs\n");
    fprintf (stderr, "       -f - Set family of subsequent UF2 files to match first\n");
    fprintf (stderr, "       -i - Ignore (preserve) different family settings\n");
    fprintf (stderr, "       -p - Set pad byte (default 0xFF)\n");
    exit (1);
    }

int main (int nArg, const char *psArg[])
    {
    FILEINFO *fi;
    FILEINFO *pfi;
    FILEINFO **pfs;
    FILE *fOut;
    FILE *fIn;
    const char *psOut = NULL;
    uint32_t iBuff[BLK_SIZE / 4];
    uint32_t iFamily = 0;
    uint32_t nBlkSize = 0;
    uint32_t uPad = 0xFF;
    int iErr = 0;
    int nFIn = 0;
    int bEra = 0;
    int bIgn = 0;
    int bFix = 0;
    int i = 1;
    while (i < nArg)
        {
        if (! strcmp (psArg[i], "-h"))
            {
            help (psArg[0]);
            }
        else if (! strcmp (psArg[i], "-e"))
            {
            bEra = 1;
            }
        else if (! strcmp (psArg[i], "-f"))
            {
            bFix = 1;
            bIgn = 0;
            }
        else if (! strcmp (psArg[i], "-i"))
            {
            bFix = 0;
            bIgn = 1;
            }
        else if (! strcmp (psArg[i], "-o"))
            {
            ++i;
            if (i >= nArg) help (psArg[0]);
            psOut = psArg[i];
            }
        else if (! strcmp (psArg[i], "-p"))
            {
            ++i;
            if (i >= nArg) help (psArg[0]);
            sscanf (psArg[i], "%i", &uPad);
            }
        else
            {
            ++nFIn;
            }
        ++i;
        }
    if (psOut == NULL)
        {
        fprintf (stderr, "No output file.\n");
        help (psArg[0]);
        }
    if (nFIn == 0)
        {
        fprintf (stderr, "No input fils.\n");
        help (psArg[0]);
        }
    fi = (FILEINFO *) malloc (nFIn * sizeof (FILEINFO));
    pfs = (FILEINFO **) malloc (nFIn * sizeof (FILEINFO *));
    if (( fi == NULL ) || ( pfs == NULL ))
        {
        fprintf (stderr, "Memory allocation failure\n");
        exit (1);
        }
    i = 1;
    pfi = fi;
    while (i < nArg)
        {
        if (psArg[i][0] == '-')
            {
            if (strchr ("op", psArg[i][1])) ++i;
            }
        else
            {
            pfi->iArg = i;
            ++pfi;
            }
        ++i;
        }
    pfi = fi;
    for (i = 0; i < nFIn; ++i)
        {
        fIn = fopen (psArg[pfi->iArg], "rb");
        if ( fIn == NULL )
            {
            fprintf (stderr, "Unable to open input file \"%s\"\n", psArg[pfi->iArg]);
            iErr = 1;
            break;
            }
        int iBlkIn = 0;
        size_t nByte = fread (iBuff, 1, BLK_SIZE, fIn);
        if ( nByte < BLK_SIZE )
            {
            fprintf (stderr, "Read error on \"%s\"\n", psArg[pfi->iArg]);
            iErr = 1;
            break;
            }
        if ( ( iBuff[0] != MAGIC_0 ) || ( iBuff[1] != MAGIC_1 ) || ( iBuff[BLK_END] != MAGIC_2 ) )
            {
            fprintf (stderr, "Block %d of \"%s\" is not valid UF2 data", iBlkIn, psArg[pfi->iArg]);
            iErr = 1;
            break;
            }
        if ( i == 0 )
            {
            nBlkSize = iBuff[4];
            if ( iBuff[2] & FLG_FAMILY )
                {
                iFamily = iBuff[7];
                }
            }
        else
            {
            if ( iBuff[4] != nBlkSize )
                {
                fprintf (stderr, "Inconsistent block size in \"%s\": %d %d\n", psArg[pfi->iArg], nBlkSize, iBuff[4]);
                iErr = 1;
                break;
                }
            if ( iFamily != 0 )
                {
                if ( bFix || (( iBuff[2] & FLG_FAMILY ) == 0 ))
                    {
                    iBuff[2] |= FLG_FAMILY;
                    iBuff[7] = iFamily;
                    }
                else if ((! bIgn) && ( iBuff[7] != iFamily ))
                    {
                    fprintf (stderr, "Inconsistent family definition in \"%s\": 0x%08X 0x%08X\n",
                        psArg[pfi->iArg], iFamily, iBuff[7]);
                    iErr = 1;
                    break;
                    }
                }
            }
        if ( iBuff[3] % nBlkSize != 0 )
            {
            fprintf (stderr, "Start address is not a multiple of block size in \"%s\"\n", psArg[pfi->iArg]);
            iErr = 1;
            break;
            }
        pfs[i] = pfi;
        pfi->nBlocks  = iBuff[6];
        pfi->iAddrMin = iBuff[3];
        pfi->iAddrMax = iBuff[3] + iBuff[6] * iBuff[4];
        // printf ("%s 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n", psArg[iArg], iBuff[3], iBuff[4], iBuff[6], pfi->iAddrMin, pfi->iAddrMax);
        fclose (fIn);
        ++pfi;
        }
    if ( iErr != 0 ) return iErr;
    bool bSorted = false;
    while ( ! bSorted )
        {
        bSorted = true;
        for (int i = 0; i < nFIn - 1; ++i)
            {
            if ( pfs[i+1]->iAddrMin < pfs[i]->iAddrMin )
                {
                bSorted = false;
                pfi = pfs[i+1];
                pfs[i+1] = pfs[i];
                pfs[i] = pfi;
                }
            }
        }
    uint32_t nBlocks = 0;
    pfs[0]->iAddrBegin = pfs[0]->iAddrMin & SECTOR_MASK;
    for (int i = 0; i < nFIn - 1; ++i)
        {
        if ( pfs[i+1]->iAddrMin < pfs[i]->iAddrMax )
            {
            fprintf (stderr, "File \"%s\" overlaps file \"%s\".\n", psArg[pfs[i+1]->iArg], psArg[pfs[i]->iArg]);
            return 1;
            }
        pfs[i]->iAddrEnd = (pfs[i]->iAddrMax + SECTOR_SIZE - 1) & SECTOR_MASK;
        pfs[i+1]->iAddrBegin = pfs[i+1]->iAddrMin & SECTOR_MASK;
        if (bEra || (pfs[i+1]->iAddrBegin < pfs[i]->iAddrEnd))
            {
            pfs[i]->iAddrEnd = pfs[i]->iAddrMax;
            pfs[i+1]->iAddrBegin = pfs[i]->iAddrMax;
            }
        nBlocks += (pfs[i]->iAddrEnd - pfs[i]->iAddrBegin) / nBlkSize;
        }
    pfs[nFIn - 1]->iAddrEnd = (pfs[nFIn - 1]->iAddrMax + SECTOR_SIZE - 1) & SECTOR_MASK;
    nBlocks += (pfs[nFIn - 1]->iAddrEnd - pfs[nFIn - 1]->iAddrBegin) / nBlkSize;
    fOut = fopen (psOut, "wb");
    if ( fOut == NULL )
        {
        fprintf (stderr, "Unable to open output file \"%s\"\n", psOut);
        return 1;
        }
    int iBlkOut = 0;
    int iArg = pfs[0]->iArg;
    for (int i = 0; i < nFIn; ++i)
        {
        pfi = pfs[i];
        uint32_t iAddrMax = pfi->iAddrBegin;
        if ( pfi->iAddrMin > pfi->iAddrBegin )
            {
            int nPad = ( pfi->iAddrMin - pfi->iAddrBegin ) / nBlkSize;
            iBuff[4] = nBlkSize;
            iBuff[6] = nBlocks;
            if ( iFamily == 0 ) iBuff[7] = nBlocks * nBlkSize;
            memset (&iBuff[8], uPad, nBlkSize);
            for (int i = 0; i < nPad; ++i)
                {
                iBuff[3] = iAddrMax;
                iBuff[5] = iBlkOut;
                // printf ("Pad block: 0x%08X 0x%08X 0x%08X 0x%08X\n", iBuff[3], iBuff[4], iBuff[5], iBuff[6]);
                if ( fwrite (iBuff, 1, BLK_SIZE, fOut) != BLK_SIZE )
                    {
                    fprintf (stderr, "Write error on \"%s\"\n", psArg[1]);
                    iErr = 1;
                    break;
                    }
                ++iBlkOut;
                iAddrMax += nBlkSize;
                }
            }
        if ( iErr != 0 ) break;
        iArg = pfi->iArg;
        fIn = fopen (psArg[iArg], "rb");
        if ( fIn == NULL )
            {
            fprintf (stderr, "Unable to open input file \"%s\"\n", psArg[iArg]);
            iErr = 1;
            break;
            }
        int iBlkIn = 0;
        while (true)
            {
            size_t nByte = fread (iBuff, 1, BLK_SIZE, fIn);
            if ( nByte == 0 ) break;
            if ( nByte < BLK_SIZE )
                {
                fprintf (stderr, "Read error on \"%s\"\n", psArg[iArg]);
                iErr = 1;
                break;
                }
            if ( ( iBuff[0] != MAGIC_0 ) || ( iBuff[1] != MAGIC_1 ) || ( iBuff[BLK_END] != MAGIC_2 ) )
                {
                fprintf (stderr, "Block %d of \"%s\" is not valid UF2 data", iBlkIn, psArg[iArg]);
                iErr = 1;
                break;
                }
            if ( iFamily != 0 )
                {
                if ( bFix || (( iBuff[2] & FLG_FAMILY ) == 0 ))
                    {
                    iBuff[2] |= FLG_FAMILY;
                    iBuff[7] = iFamily;
                    }
                else if ((! bIgn) && ( iBuff[7] != iFamily ))
                    {
                    fprintf (stderr, "Inconsistent family definition in \"%s\": 0x%08X 0x%08X\n",
                        psArg[iArg], iFamily, iBuff[7]);
                    iErr = 1;
                    break;
                    }
                }
            iBuff[4] = nBlkSize;
            iBuff[5] = iBlkOut;
            iBuff[6] = nBlocks;
            iAddrMax = iBuff[3] + nBlkSize;
            if ( iFamily == 0 ) iBuff[7] = nBlocks * nBlkSize;
            // printf ("Data block: 0x%08X 0x%08X 0x%08X 0x%08X\n", iBuff[3], iBuff[4], iBuff[5], iBuff[6]);
            if ( fwrite (iBuff, 1, BLK_SIZE, fOut) != BLK_SIZE )
                {
                fprintf (stderr, "Write error on \"%s\"\n", psArg[1]);
                iErr = 1;
                break;
                }
            ++iBlkIn;
            ++iBlkOut;
            }
        fclose (fIn);
        if ( iErr != 0 ) break;
        if ( pfi->iAddrEnd > pfi->iAddrMax )
            {
            int nPad = ( pfi->iAddrEnd - pfi->iAddrMax ) / nBlkSize;
            iBuff[4] = nBlkSize;
            iBuff[6] = nBlocks;
            if ( iFamily == 0 ) iBuff[7] = nBlocks * nBlkSize;
            memset (&iBuff[8], uPad, nBlkSize);
            for (int i = 0; i < nPad; ++i)
                {
                iBuff[3] = iAddrMax;
                iBuff[5] = iBlkOut;
                // printf ("Pad block: 0x%08X 0x%08X 0x%08X 0x%08X\n", iBuff[3], iBuff[4], iBuff[5], iBuff[6]);
                if ( fwrite (iBuff, 1, BLK_SIZE, fOut) != BLK_SIZE )
                    {
                    fprintf (stderr, "Write error on \"%s\"\n", psArg[1]);
                    iErr = 1;
                    break;
                    }
                ++iBlkOut;
                iAddrMax += nBlkSize;
                }
            }
        if ( iErr != 0 ) break;
        printf ("%3d: 0x%08X - 0x%08X (0x%08X - 0x%08X) %s\n", i + 1, pfi->iAddrMin, pfi->iAddrMax,
            pfi->iAddrBegin, pfi->iAddrEnd, psArg[iArg]);
        }
    fclose (fOut);
    return iErr;
    }
