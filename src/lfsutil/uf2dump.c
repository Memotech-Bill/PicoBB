/* uf2dump.c - Program to extact binary data from UF2 file */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

int main (int nArg, const char *psArg[])
    {
    FILE *fIn;
    FILE *fOut = NULL;
    int nImage = 0;
    uint32_t addrStart;
    uint32_t addrNext = 0xFFFFFFFF;
    struct UF2_Block
        {
        // 32 byte header
        uint32_t magicStart0;
        uint32_t magicStart1;
        uint32_t flags;
        uint32_t targetAddr;
        uint32_t payloadSize;
        uint32_t blockNo;
        uint32_t numBlocks;
        uint32_t fileSize; // or familyID;
        uint8_t data[476];
        uint32_t magicEnd;
        } blk;
    if (nArg < 2)
        {
        fprintf (stderr, "Usage %s <uf2 file> [<image file> [...]]\n", psArg[0]);
        return 1;
        }
    fIn = fopen (psArg[1], "rb");
    if (fIn == NULL)
        {
        fprintf (stderr, "Failed to open UF2 file: %s\n", psArg[1]);
        return 1;
        }
    while (fread (&blk, sizeof (blk), 1, fIn))
        {
        if ((blk.magicStart0 != 0x0A324655) || (blk.magicStart1 != 0x9E5D5157) || (blk.magicEnd != 0x0AB16F30))
            {
            fprintf (stderr, "Invalid UF2 file.\n");
            return 2;
            }
        // printf ("Flags = 0x%04X, Addr = 0x%08X, Payload = %d, Block No = %d, Num Blocks = %d, Size = 0x%08X\n",
        //     blk.flags, blk.targetAddr, blk.payloadSize, blk.blockNo, blk.numBlocks, blk.fileSize);
        if (blk.targetAddr != addrNext)
            {
            if (nImage > 0)
                {
                printf ("Image %d, Start Address = 0x%08X, End Address = 0x%08X", nImage, addrStart, addrNext);
                if (fOut != NULL)
                    {
                    fclose (fOut);
                    fOut = NULL;
                    printf (", Saved as %s.", psArg[nImage+1]);
                    }
                printf ("\n");
                }
            ++nImage;
            addrStart = blk.targetAddr;
            if ((nImage + 1 < nArg) && (psArg[nImage+1][0] != '\0') && (psArg[nImage+1][0] != '-'))
                {
                fOut = fopen (psArg[nImage+1], "wb");
                if (fOut == NULL)
                    {
                    fprintf (stderr, "Failed to open image file: %s\n", psArg[nImage+1]);
                    return 1;
                    }
                }
            }
            if (fOut != NULL) fwrite (blk.data, blk.payloadSize, 1, fOut);
            addrNext = blk.targetAddr + blk.payloadSize;
        }
    if (nImage > 0)
        {
        printf ("Image %d, Start Address = 0x%08X, End Address = 0x%08X", nImage, addrStart, addrNext);
        if (fOut != NULL)
            {
            fclose (fOut);
            printf (", Saved as %s.", psArg[nImage+1]);
            }
        printf ("\n");
        }
    fclose (fIn);
    return 0;
    }
