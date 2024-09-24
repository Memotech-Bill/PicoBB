#!/usr/bin/python3
#
# A program to adjust the PicoBB memory map for custom builds
#
import sys

def MemMap (sOut, sIn, sMem, sOS):
    print ('Generating link file "{:s}" from "{:s}" for {:s} / {:s}kB of OS / Total RAM.'.format (sOut, sIn, sOS, sMem))
    total_mem = int (sMem, 10);
    os_size = int (sOS, 10)
    core_2_size = 2
    basic_size = total_mem - os_size - core_2_size
    basic_orig = 0x20000000 + 1024 * os_size
    core_2_orig = 0x20000000 + 1024 * (os_size + basic_size)
    with open (sIn, 'r') as fIn, open (sOut, 'w') as fOut:
        for sLine in fIn:
            lPart = sLine.split (maxsplit = 1)
            if len (lPart) == 0:
                fOut.write (sLine)
            elif lPart[0].startswith ('RAM(rwx)'):
                n = sLine.index (':') + 1
                fOut.write (sLine[0:n])
                fOut.write (' ORIGIN = 0x{:X}, LENGTH = {:d}k\n'.format (0x20000000, os_size))
            elif lPart[0].startswith ('SCRATCH_Y(rwx)'):
                n = sLine.index (':') + 1
                fOut.write (sLine[0:n])
                fOut.write (' ORIGIN = 0x{:X}, LENGTH = {:d}k\n'.format (basic_orig, basic_size))
            elif lPart[0].startswith ('SCRATCH_X(rwx)'):
                n = sLine.index (':') + 1
                fOut.write (sLine[0:n])
                fOut.write (' ORIGIN = 0x{:X}, LENGTH = {:d}k\n'.format (core_2_orig, core_2_size))
            else:
                fOut.write (sLine)

if ( len (sys.argv) == 5 ):
    MemMap (sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
else:
    print ('Usage: memmap.py <output link file> <input link file> <RAM size> <OS size>')
