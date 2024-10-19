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

#define XIP_BASE    0x10000000



const char *psName;

void error (const char *psErr)
    {
    fprintf (stderr, "%s", psErr);
    fprintf (stderr, "USAGE: %s [-d ABSOLUTE|2040|DATA|2350[_ASM_S]|2350_RISCV|2350_ARM_NS] [-f <id>] [-m ref.uf2] file.bin file.uf2 [address]\n", psName);
    exit (1);
    }

uint32_t parse_address (const char *ps)
    {
    uint32_t addr = 0;
    while (*ps)
        {
        if ((*ps >= '0') && (*ps <= '9')) addr = 10 * addr + *ps - '0';
        else break;
        ++ps;
        }
    if ((*ps == 'K') || (*ps == 'k')) {addr *= 1024; ++ps;}
    else if ((*ps == 'M') || (*ps == 'm')) {addr *= 1024 * 1024; ++ps;}
    if ((*ps) || (addr == 0))
        {
        fprintf (stderr, "Invalid Address\n");
        exit (1);
        }
    return  addr + XIP_BASE;
    }

int main(int argc, char** argv) {
    const char *psIn = NULL;
    const char *psOut = NULL;
    const char *psRef = NULL;
    FILE* f;
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
                if (! strcasecmp (argv[i], "ABSOLUTE"))         family = ABSOLUTE_FAMILY_ID;
                else if (! strcasecmp (argv[i], "2040"))        family = RP2040_FAMILY_ID;
                else if (! strcasecmp (argv[i], "DATA"))        family = DATA_FAMILY_ID;
                else if (! strcasecmp (argv[i], "2350"))        family = RP2350_ARM_S_FAMILY_ID;
                else if (! strcasecmp (argv[i], "2350_ARM_S"))  family = RP2350_ARM_S_FAMILY_ID;
                else if (! strcasecmp (argv[i], "2350_RISCV"))  family = RP2350_RISCV_FAMILY_ID;
                else if (! strcasecmp (argv[i], "2350_ARM_NS")) family = RP2350_ARM_NS_FAMILY_ID;
                else error ("Unrecognised device\n");
                flag = '\0';
                break;
            case 'f':
                family = strtoul(argv[i], NULL, 0);
                flag = '\0';
                break;
            case 'm':
                psRef = argv[i];
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
                    else if (nArg == 2) address = parse_address (argv[i]);
                    else error ("Too many arguments\n");
                    ++nArg;
                    }
                break;
            }
        }
    if (psIn == NULL) error ("No input file specified\n");
    if (psOut == NULL) error ("No output file specified\n");

    UF2_Block bl;
    memset(&bl, 0, sizeof(bl));

    if (psRef != NULL)
        {
        f = fopen (psRef, "rb");
        if (!f)
            {
            fprintf(stderr, "No reference file: %s\n", psRef);
            return 1;
            }
        if (fread (&bl, sizeof (uint32_t), 8, f) != 8)
            {
            fprintf(stderr, "Error reading reference file: %s\n", psRef);
            fclose (f);
            return 1;
            }
        fclose (f);
        if (bl.flags & UF2_FLAG_FAMILY_ID_PRESENT) family = bl.fileSize;
        }
    else
        {
        bl.magicStart0 = UF2_MAGIC_START0;
        bl.magicStart1 = UF2_MAGIC_START1;
        bl.flags = family ? UF2_FLAG_FAMILY_ID_PRESENT : 0;
        bl.fileSize = family;
        }


    f = fopen(psIn, "rb");
    if (!f)
        {
        fprintf(stderr, "No such file: %s\n", psIn);
        return 1;
        }

    fseek(f, 0L, SEEK_END);
    uint32_t sz = ftell(f);
    fseek(f, 0L, SEEK_SET);

    bl.targetAddr = address;
    bl.numBlocks = (sz + 255) / 256;
    bl.payloadSize = 256;
    bl.magicEnd = UF2_MAGIC_END;

    FILE* fout = fopen(psOut, "wb");
    if (!fout)
        {
        fprintf(stderr, "Unable to create output file: %s\n", psOut);
        return 1;
        }
    
    int numbl = 0;
    while (fread(bl.data, 1, bl.payloadSize, f))
        {
        bl.blockNo = numbl++;
        fwrite(&bl, 1, sizeof(bl), fout);
        bl.targetAddr += bl.payloadSize;
        // clear for next iteration, in case we get a short read
        memset(bl.data, 0, sizeof(bl.data));
        }
    fclose(fout);
    fclose(f);
    printf("Wrote %d blocks to \"%s\" from \"%s\", load address = 0x%08X", numbl, psOut, psIn, address);
    if (family) printf (", family ID = 0x%08X", family);
    printf ("\n");
    return 0;
}
