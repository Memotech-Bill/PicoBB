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

typedef struct
    {
    int         iArg;
    uint32_t    nBlocks;
    uint32_t    iAddrMin;
    uint32_t    iAddrMax;
    } FILEINFO;

int main (int nArg, const char *psArg[])
    {
    FILEINFO *fi;
    FILEINFO *pfi;
    FILEINFO **pfs;
    FILE *fOut;
    FILE *fIn;
    uint32_t iBuff[BLK_SIZE / 4];
    uint32_t iFamily = 0;
    uint32_t nBlkSize = 0;
    int iErr = 0;
    if ( nArg < 3 )
        {
        fprintf (stderr, "Usage: %s output.uf2 input1.uf2 input2.uf2...\n", psArg[0]);
        exit (1);
        }
    fi = (FILEINFO *) malloc ((nArg - 2) * sizeof (FILEINFO));
    pfs = (FILEINFO **) malloc ((nArg - 2) * sizeof (FILEINFO *));
    if (( fi == NULL ) || ( pfs == NULL ))
        {
        fprintf (stderr, "Memory allocation failure\n");
        exit (1);
        }
    pfi = fi;
    for (int iArg = 2; iArg < nArg; ++iArg)
        {
        fIn = fopen (psArg[iArg], "rb");
        if ( fIn == NULL )
            {
            fprintf (stderr, "Unable to open input file \"%s\"\n", psArg[iArg]);
            iErr = 1;
            break;
            }
        int iBlkIn = 0;
        size_t nByte = fread (iBuff, 1, BLK_SIZE, fIn);
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
        if ( iArg == 2 )
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
                fprintf (stderr, "Inconsistent block size in \"%s\"\n", psArg[iArg]);
                iErr = 1;
                break;
                }
            if ( iFamily != 0 )
                {
                if ( ( ( iBuff[2] & FLG_FAMILY ) != FLG_FAMILY ) || ( iBuff[7] != iFamily ) )
                    {
                    fprintf (stderr, "Inconsistent family definition in \"%s\"\n", psArg[iArg]);
                    iErr = 1;
                    break;
                    }
                }
            }
        if ( iBuff[3] % nBlkSize != 0 )
            {
            fprintf (stderr, "Start address is not a multiple of block size in \"%s\"\n", psArg[iArg]);
            iErr = 1;
            break;
            }
        pfs[iArg-2] = pfi;
        pfi->iArg     = iArg;
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
        for (int i = 0; i < nArg - 3; ++i)
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
    uint32_t nBlocks = ( pfs[nArg-3]->iAddrMax - pfs[0]->iAddrMin ) / nBlkSize;
    fOut = fopen (psArg[1], "wb");
    if ( fOut == NULL )
        {
        fprintf (stderr, "Unable to open output file \"%s\"\n", psArg[1]);
        return 1;
        }
    int iBlkOut = 0;
    int iArg = pfs[0]->iArg;
    uint32_t iAddrMax = pfs[0]->iAddrMin;
    for (int i = 0; i < nArg - 2; ++i)
        {
        pfi = pfs[i];
        if ( pfi->iAddrMin < iAddrMax )
            {
            fprintf (stderr, "File \"%s\" overlaps file \"%s\".", psArg[pfs[i]->iArg], psArg[iArg]);
            return 1;
            }
        else if ( pfi->iAddrMin > iAddrMax )
            {
            int nPad = ( pfi->iAddrMin - iAddrMax ) / nBlkSize;
            iBuff[4] = nBlkSize;
            iBuff[6] = nBlocks;
            if ( iFamily == 0 ) iBuff[7] = nBlocks * nBlkSize;
            memset (&iBuff[8], 0, nBlkSize);
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
                if ( ( ( iBuff[2] & FLG_FAMILY ) != FLG_FAMILY ) || ( iBuff[7] != iFamily ) )
                    {
                    fprintf (stderr, "Inconsistent family definition in \"%s\"\n", psArg[iArg]);
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
        printf ("%3d: 0x%08X - 0x%08X %s\n", iArg - 1, pfi->iAddrMin, pfi->iAddrMax, psArg[iArg]);
        }
    fclose (fOut);
    return iErr;
    }
