#!/usr/bin/python3
#
# Merge lists of symbols and generate an include file
#
import sys

# Exclude routines that are defined in sympico.c
exclude = ['sympico', 'strcmp', 'tud_cdc_connected']

class symbols:
    def __init__ (self):
        self.names = []
        self.map = {}

    def load (self, sIn):
        with open (sIn, 'r') as fIn:
            for sLine in fIn:
                lParts = sLine.strip ().split ()
                self.names.append (lParts[0])
                if ( len (lParts) > 1 ):
                    self.map[lParts[0]] = lParts[1]

    def load_all (self, lSym):
        for sIn in lSym:
            self.load (sIn)

    def save (self, sOut):
        self.names.sort ()
        with open (sOut, 'w') as fOut:
            fOut.write ('// sympico.c - Declare functions accessable via SYM\n'
                        '// Automatically generated - do not edit.\n\n'
                        '#pragma GCC diagnostic push\n'
                        '#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"\n\n')
            sLast = ''
            for sName in self.names:
                if ( sName != sLast ):
                    sLast = sName
                    sRtn = self.map.get (sName, sName)
                    if ( sRtn not in exclude ):
                        fOut.write ('void ' + sRtn + ' (void);\n')
            fOut.write ('\n#pragma GCC diagnostic pop\n\n'
                        '\ntypedef struct st_symbols\n'
                        '    {\n'
                        '    const char *s;\n'
                        '    void *p;\n'
                        '    } symbols;\n'
                        '\nconst symbols sdkfuncs[] = {\n')
            sLast = ''
            for sName in self.names:
                if ( sName != sLast ):
                    sLast = sName
                    sRtn = self.map.get (sName, sName)
                    fOut.write ('    {"' + sName + '", ' + sRtn + '},\n')
            fOut.write ('};\n')

    def process (self, argv):
        self.load_all (argv[2:])
        self.save (argv[1])

if ( len (sys.argv) > 1 ):
    symbols ().process (sys.argv)
else:
    printf ('Usage: merge_funcs.py <include file> <symbols file>...')
    sys.exit (1)
