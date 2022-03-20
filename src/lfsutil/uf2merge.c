/*  uf2merge.c - Merge multiple UF2 files into a single file

Usage: uf2merge output.uf2 input1.uf2 input2.uf2

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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
    uint32_t    nBlocks;
    uint32_t    iAddrMin;
    uint32_t    iAddrMax;
    } FILEINFO;

int main (int nArg, const char *psArg[])
    {
    FILEINFO *fi;
    FILEINFO *pfi;
    FILE *fOut;
    FILE *fIn;
    uint32_t iBuff[BLK_SIZE / 4];
    uint32_t iFamily = 0;
    uint32_t nBlocks = 0;
    struct stat st;
    if ( nArg < 3 )
        {
        fprintf (stderr, "Usage: %s output.uf2 input1.uf2 input2.uf2...\n", psArg[0]);
        exit (1);
        }
    fi = (FILEINFO *) malloc ((nArg - 2) * sizeof (FILEINFO));
    if ( fi == NULL )
        {
        fprintf (stderr, "Memory allocation failure\n");
        exit (1);
        }
    pfi = fi;
    for (int iArg = 2; iArg < nArg; ++iArg)
        {
        if ( stat (psArg[iArg], &st) != 0 )
            {
            fprintf (stderr, "Unable to stat file \"%s\"\n", psArg[iArg]);
            exit (1);
            }
        pfi->nBlocks = st.st_size / BLK_SIZE;
        if ( st.st_size != BLK_SIZE * pfi->nBlocks )
            {
            fprintf (stderr, "File \"%s\" is not a whole number of blocks\n", psArg[iArg]);
            exit (1);
            }
        nBlocks += pfi->nBlocks;
        ++pfi;
        }
    fOut = fopen (psArg[1], "wb");
    if ( fOut == NULL )
        {
        fprintf (stderr, "Unable to open output file \"%s\"\n", psArg[1]);
        exit (1);
        }
    int iErr = 0;
    pfi = fi;
    int iBlkOut = 0;
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
            if ( iBlkOut == 0 )
                {
                if ( iBuff[2] & FLG_FAMILY )
                    {
                    iFamily = iBuff[7];
                    }
                }
            else if ( iFamily != 0 )
                {
                if ( ( ( iBuff[2] & FLG_FAMILY ) != FLG_FAMILY ) || ( iBuff[7] != iFamily ) )
                    {
                    fprintf (stderr, "Inconsistent family definition in \"%s\"\n", psArg[iArg]);
                    iErr = 1;
                    break;
                    }
                }
            uint32_t iAddrMin = iBuff[3];
            uint32_t iAddrMax = iBuff[3] + iBuff[4];
            if ( iBlkIn == 0 )
                {
                pfi->iAddrMin = iAddrMin;
                pfi->iAddrMax = iAddrMax;
                }
            else
                {
                if ( iAddrMin < pfi->iAddrMin ) pfi->iAddrMin = iAddrMin;
                if ( iAddrMax > pfi->iAddrMax ) pfi->iAddrMax = iAddrMax;
                }
            iBuff[5] = iBlkOut;
            iBuff[6] = nBlocks;
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
        for ( int iFil = 0; iFil < iArg - 3; ++iFil )
            {
            if ( ( pfi->iAddrMin < fi[iFil].iAddrMax ) && ( pfi->iAddrMax > fi[iFil].iAddrMin ) )
                {
                printf ("   Overlaps with \"%s\"\n", psArg[iFil+2]);
                }
            }
        }
    fclose (fOut);
    return iErr;
    }
