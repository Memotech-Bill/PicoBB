#!/usr/bin/python3
#
# Obtain lists of function names from header files
#
import sys
import os

# Functions which are dependent on non-default #defines
# exclude_funcs = ['vfctprintf', 'queue_get_max_level', 'queue_reset_max_level']

def Clean (sLine):
    nCh = sLine.find ('/*')
    if ( nCh < 0 ):
        nCh = sLine.find ('//')
    if ( nCh > 0 ):
        sLine = sLine[0:nCh]
    return sLine.strip ()

def Body (sLine):
    nCh = sLine.find ('/*')
    if ( nCh < 0 ):
        nCh = sLine.find ('//')
    if ( nCh > 0 ):
        sLine = sLine[0:nCh]
    return sLine.split (None, 1)[1].strip ()

class Header:
    def __init__ (self, sIn, fOut, fStub, gldef):
        self.sIn = sIn
        nCh = sIn.find ('/include/')
        if ( nCh >= 0 ):
            self.sHeader = sIn[nCh + 9:]
        else:
            self.sHeader = sIn
        self.fOut = fOut
        self.fStub = fStub
        self.defs = {}
        self.excl = set ()
        self.active = [[True, True]]
        self.nBrace = 0
        self.sPrefix = ''
        self.bFunc = False
        self.bStub = False
        for sLine in gldef:
            self.define (sLine)

    def define (self, sLine):
        sLine = Body (sLine)
        if ( sLine[0] == '"' ):
            nCh = sLine.rfind ('"')
            name = sLine[1:nCh].strip ()
            if ( nCh < len (sLine) ):
                value = sLine[nCh+1:].strip ()
            else:
                value = '1'
        else:
            lparts = sLine.split (None, 1)
            name = lparts[0]
            if ( len (lparts) == 2 ):
                value = lparts[1]
            else:
                value = '1'
        if ( value.isdecimal () ):
            value = int (value, 10)
        else:
            value = self.defs.get (value, False)
        self.defs[name] = value
        #self.fOut.write ('#define: name = ' + name + ', value = ' + str(value) + '\n')

    def exclude (self, sLine):
        self.excl.add (sLine.split (None, 1)[1].strip ())

    def do_if (self, sLine):
        sLine = Body (sLine)
        do = self.defs.get (sLine, False) and self.active[-1][0]
        self.active.append ([do, do])

    def do_elseif (self, sLine):
        sLine = Body (sLine)
        do = self.defs.get (sLine, False) and self.active[-2][0]
        self.active[-1][0] = do
        if ( do ):
            self.active[-1][1] = True

    def do_else (self):
        self.active[-1][0] = self.active[-2][0] and not self.active[-1][1]

    def do_endif (self):
        del self.active[-1]

    def do_ifdef (self, sLine):
        sLine = Body (sLine)
        do = sLine in self.defs
        self.active.append ([do, do])

    def do_ifndef (self, sLine):
        sLine = Body (sLine)
        do = sLine not in self.defs
        self.active.append ([do, do])

    def do_func (self, sLine):
        nCh = sLine.find ('(')
        lTerms = sLine[0:nCh].split ()
        if (( len (lTerms) > 1 )
            and ( not lTerms[-1].startswith ('__') )
            and ( not lTerms[-1].startswith ('weak') )
            and ( not lTerms[-1].endswith ('_unsafe') )
            and ( not lTerms[-1] in self.excl )):
            sPtr = ''
            if ( lTerms[-1][0] == '*' ):
                lTerms[-1] = lTerms[-1][1:]
                sPtr = '*'
            if (( self.nBrace == 0 ) and ( lTerms[0] not in ['static', 'extern', 'typedef'] )):
                if ( not self.bFunc ):
                    self.fOut.write ('#\n# From file ' + self.sIn + '\n')
                    self.bFunc = True
                self.fOut.write (lTerms[-1] + '\n')
            elif (( lTerms[0] == 'static' ) and ( len (lTerms) > 3 )):
                nCh2 = sLine.rfind (')')
                if ( nCh2 > 0 ):
                    if ( not self.bStub ):
                        self.fStub.write ('\n#include "' + self.sHeader + '"\n')
                        self.bStub = True
                    lParm = sLine[nCh+1:nCh2].split (',')
                    self.fStub.write ('\n' + (' '.join (lTerms[2:-1])) + ' ')
                    self.fStub.write (sPtr + 'stub_' + lTerms[-1] + ' ')
                    self.fStub.write (sLine[nCh:nCh2+1] + '\n    {\n    ')
                    if ( lTerms[2] != 'void' ):
                        self.fStub.write ('return ')
                    self.fStub.write (lTerms[-1])
                    for i in range (len (lParm)):
                        if ( lParm[i][-1] == ')' ):
                            lParm[i] = lParm[i][lParm[i].find ('(*')+2:lParm[i].find (')')]
                        else:
                            lParm[i] = lParm[i].strip ().split ()[-1].replace ('*', '')
                        if ( lParm[i] == 'void' ):
                            lParm[i] = ''
                    self.fStub.write (' (' + (', '.join (lParm)) + ');\n    }\n')
                    if ( not self.bFunc ):
                        self.fOut.write ('#\n# From file ' + self.sIn + '\n')
                        self.bFunc = True
                    self.fOut.write (lTerms[-1] + '\tstub_' + lTerms[-1] + '\n')
                else:
                    self.sPrefix = sLine + ' '

    def Parse (self):
        with open (self.sIn, 'r') as fIn:
            bInCmnt = False
            for sLine in fIn:
                sLine = self.sPrefix + sLine.strip ()
                self.sPrefix = ''
                if ( bInCmnt ):
                    if ( sLine.endswith ('*/') ):
                        bInCmnt = False
                elif ( sLine.startswith ('/*') ):
                    bInCmnt = not sLine.endswith ('*/')
                elif ( sLine.startswith ('//') ):
                    pass
                elif ( sLine.startswith ('#') ):
                    if ( sLine.endswith ('\\') ):
                        self.sPrefix = sLine[0:-1]
                    elif ( sLine.startswith ('#ifdef') ):
                        self.do_ifdef (sLine)
                    elif ( sLine.startswith ('#ifndef') ):
                        self.do_ifndef (sLine)
                    elif ( sLine.startswith ('#if') ):
                        self.do_if (sLine)
                        #self.fOut.write (sLine + ' active = ' + str (self.active[-1][0]) + '\n')
                    elif ( sLine.startswith ('#elseif') ):
                        self.do_elseif (sLine)
                    elif ( sLine.startswith ('#else') ):
                        self.do_else ()
                    elif ( sLine.startswith ('#endif') ):
                        self.do_endif ()
                    elif ( sLine.startswith ('#define') ):
                        self.define (sLine)
                    # self.fOut.write (sLine + ' active = ' + str (self.active[-1][0]) + '\n')
                elif ( self.active[-1][0] ):
                    sLine = Clean (sLine)
                    self.nBrace += sLine.count ('{')
                    if ( sLine == 'extern "C" {' ):
                        self.nBrace -= 1
                    if ( '(' in sLine ):
                        self.do_func (sLine)
                    self.nBrace -= sLine.count ('}')
                    if ( self.nBrace < 0 ):
                        self.nBrace = 0

def header_list (sOut, sStub, lList):
    gldef = []
    with open (sOut, 'w') as fOut, open (sStub, 'w') as fStub:
        fOut.write ('# ' + sOut + ' - SDK routines for inclusion in BBC BASIC SYM table\n'
                    '# Automatically generated by header_funcs.py ' + sOut + ' ' + sStub
                    + ' ' + ' '.join (lList) + '\n# Do not edit\n')
        fStub.write ('// ' + sStub + ' - Stub functions for SDK routines that are otherwise inline\n'
                     '// Automatically generated by header_funcs.py ' + sOut + ' ' + sStub
                     + ' ' + ' '.join (lList) + '\n// Do not edit\n\n')
        for sList in lList:
            fOut.write ('#\n# Processing file ' + sList + '\n')
            with open (sList, 'r') as fList:
                hdr = None
                for sLine in fList:
                    sLine = sLine.strip ()
                    if (( len (sLine) > 0 ) and ( sLine[0] != '#' )):
                        if ( sLine.startswith ('%global') ):
                            gldef.append (sLine)
                        elif ( sLine.startswith ('%define') ):
                            hdr.define (sLine)
                        elif ( sLine.startswith ('%exclude') ):
                            hdr.exclude (sLine)
                        elif ( sLine.startswith ('%function') ):
                           lparts = sLine.split ()
                           fOut.write (lparts[1])
                           if ( len (lparts) > 2 ):
                              fOut.write ('\t' + lparts[2] + '\n')
                           else:
                              fOut.write ('\n')
                        else:
                            if ( hdr is not None ):
                                hdr.Parse ()
                            hdr = Header (os.path.expandvars (sLine), fOut, fStub, gldef)
            if ( hdr is not None ):
                hdr.Parse ()

if ( len (sys.argv) >= 4 ):
    header_list (sys.argv[1], sys.argv[2], sys.argv[3:])
else:
    print ('Usage: header_funcs.py <symbol file> <stubs file> <headers file>...')
    sys.exit (1)
