/*
 *  MIT License
 *
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE
 */

#include "uf2format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *psName;

void error (const char *psErr)
    {
    fprintf (stderr, "%s", psErr);
    fprintf (stderr, "USAGE: %s [-d 2040|2350] [-f <id>] file.bin file.uf2 [address]\n", psName);
    exit (1);
    }

int main(int argc, char** argv) {
    const char *psIn = NULL;
    const char *psOut = NULL;
    uint32_t address = 0;
    uint32_t family = 0;
    int nArg = 0;
    char flag = '\0';
    psName = argv[0];
    for (int i = 1; i < argc; ++i)
        {
        switch (flag)
            {
            case 'd':
                if (! strcmp (argv[i], "2040")) family = RP2040_FAMILY_ID;
                else if (! strcmp (argv[i], "2350")) family = RP2040_FAMILY_ID;
                else error ("Unrecognised device\n");
                flag = '\0';
                break;
            case 'f':
                family = strtoul(argv[i], NULL, 0);
                flag = '\0';
                break;
            default:
                if (flag) error ("Unrecognised switch\n");
                if (argv[i][0] == '-')
                    {
                    flag = argv[i][1];
                    }
                else
                    {
                    if (nArg == 0) psIn = argv[i];
                    else if (nArg == 1) psOut = argv[i];
                    else if (nArg == 2) address = strtoul(argv[i], NULL, 0);
                    else error ("Too many arguments\n");
                    ++nArg;
                    }
                break;
            }
        }
    if (psIn == NULL) error ("No input file specified\n");
    if (psOut == NULL) error ("No output file speccified\n");

    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "No such file: %s\n", argv[1]);
        return 1;
    }

    fseek(f, 0L, SEEK_END);
    uint32_t sz = ftell(f);
    fseek(f, 0L, SEEK_SET);

    const char* outname = argc > 2 ? argv[2] : "flash.uf2";

    FILE* fout = fopen(outname, "wb");

    UF2_Block bl;
    memset(&bl, 0, sizeof(bl));

    bl.magicStart0 = UF2_MAGIC_START0;
    bl.magicStart1 = UF2_MAGIC_START1;
    bl.magicEnd = UF2_MAGIC_END;
    bl.flags = family ? UF2_FLAG_FAMILY_ID_PRESENT : 0;
    bl.fileSize = family;
    bl.targetAddr = address;
    bl.numBlocks = (sz + 255) / 256;
    bl.payloadSize = 256;
    int numbl = 0;
    while (fread(bl.data, 1, bl.payloadSize, f)) {
        bl.blockNo = numbl++;
        fwrite(&bl, 1, sizeof(bl), fout);
        bl.targetAddr += bl.payloadSize;
        // clear for next iteration, in case we get a short read
        memset(bl.data, 0, sizeof(bl.data));
    }
    fclose(fout);
    fclose(f);
    printf("Wrote %d blocks to %s\n", numbl, outname);
    return 0;
}
