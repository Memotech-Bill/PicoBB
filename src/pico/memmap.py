#!/usr/bin/python3
#
# A program to adjust the PicoBB memory map for custom builds
#
import sys

def MemInfo (sLine):
    n1 = sLine.index ('ORIGIN') + 6
    while ( sLine[n1] == ' ' ) or ( sLine[n1] == '=' ):
        n1 += 1
    n2 = sLine.index (',', n1)
    orig = int (sLine[n1:n2], 0)
    n1 = sLine.index ('LENGTH', n2) + 6
    while ( sLine[n1] == ' ' ) or ( sLine[n1] == '=' ):
        n1 += 1
    n2 = sLine.index ('k', n1)
    size = int (sLine[n1:n2], 10)
    return (orig, size)

def MemMap (sOut, sIn, sRAM):
    print ('Generating link file "{:s}" from "{:s}" for {:s}kB of OS RAM.'.format (sOut, sIn, sRAM))
    new_ram_size = int (sRAM, 10)
    with open (sIn, 'r') as fIn, open (sOut, 'w') as fOut:
        for sLine in fIn:
            lPart = sLine.split (maxsplit = 1)
            if len (lPart) == 0:
                fOut.write (sLine)
            elif lPart[0].startswith ('RAM(rwx)'):
                ram_orig, old_ram_size = MemInfo (sLine)
                n = sLine.index (':') + 1
                fOut.write (sLine[0:n])
                fOut.write (' ORIGIN = 0x{:X}, LENGTH = {:d}k\n'.format (ram_orig, new_ram_size))
            elif lPart[0].startswith ('SCRATCH_Y(rwx)'):
                old_scratch_orig, old_scratch_size = MemInfo (sLine)
                new_scratch_orig = ram_orig + 1024 * new_ram_size
                new_scratch_size = old_scratch_size + old_ram_size - new_ram_size
                n = sLine.index (':') + 1
                fOut.write (sLine[0:n])
                fOut.write (' ORIGIN = 0x{:X}, LENGTH = {:d}k\n'.format (new_scratch_orig, new_scratch_size))
            else:
                fOut.write (sLine)

if ( len (sys.argv) == 4 ):
    MemMap (sys.argv[1], sys.argv[2], sys.argv[3])
else:
    print ('Usage: memmap.py <output link file> <input link file> <OS size>')
