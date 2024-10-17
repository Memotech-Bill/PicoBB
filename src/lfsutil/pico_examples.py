#!/usr/bin/python3
#
# Program to collects PicoBB example programs into a LFS filesystem
#
import sys
import os
import argparse
import shutil
import glob

class Example:
    def __init__ (self, src, dst):
        self.src = src
        self.dst = dst

    def Copy (self, tree):
        dst = os.path.join (tree, self.dst)
        os.makedirs (dst, exist_ok = True)
        dst = os.path.join (dst, os.path.basename (self.src))
        print ('\t{:s} --> {:s}'.format (self.src, dst))
        shutil.copyfile (self.src, dst)

def Latest (examples):
    tlast = None
    for ex in examples:
        fn = ex.src
        t = os.path.getmtime (fn)
        if ( tlast is None ) or ( t > tlast ):
            tlast = t
    return tlast

def Parse (examples, cf, dev, bld):
    if ( os.path.exists (cf) ):
        with open (cf, 'r') as f:
            basedir = os.path.dirname (cf)
            nline = 0
            select = False
            for line in f:
                nline += 1
                line = line.strip ()
                if line == '':
                    continue
                if line[0] == '#':
                    continue
                if line[0] == '[':
                    parm = line[1:-1].split (',')
                    if len (parm) != 3:
                        print ('Invalid section at line {:d} in {:s}'.format (nline, cf))
                        return
                    parm[0] = parm[0].strip ()
                    parm[1] = parm[1].strip ()
                    parm[2] = parm[2].strip ()
                    if parm[2][-1] == '/':
                        parm[2] = parm[2][0:-1]
                    select = ( parm[0] in dev ) and ( parm[1] in bld )
                elif select:
                    for fn in glob.glob (os.path.join (basedir, line)):
                        if os.path.isfile (fn):
                            examples.append (Example (fn, parm[2]))

def Build (cfg):
    if os.path.exists (cfg.output):
        tOut = os.path.getmtime (cfg.output)
        make = False
    else:
        tOut = 0
        make = True
    examples = []
    for cf in cfg.config_file:
        if os.path.getmtime (cf) > tOut:
            make = True
        Parse (examples, cf, cfg.device, cfg.build)
    if Latest (examples) > tOut:
        make = True
    if make:
        for ex in examples:
            ex.Copy (cfg.tree)
        err = os.system (os.path.join (os.path.dirname (sys.argv[0]), 'mklfsimage') + ' -o ' + cfg.output
                   + ' -s ' + cfg.size + ' ' + cfg.tree)
        if err != 0:
            if err & 0x7F == 0:
                err >>= 8
            else:
                err = -(err & 0x7F)
            sys.stderr.write ('ERROR: Failed to create LFS filesystem\n')
            sys.exit (err)

def Run ():
    if ( len (sys.argv) == 1 ):
        sys.argv.append ('-h')
    parser = argparse.ArgumentParser (description = 'Collect BASIC example programs into directory tree')
    parser.add_argument ('-v', '--version', action = 'version', version = '%(prog)s v241017')
    parser.add_argument ('-d', '--device', action = 'append', help = 'Device to assemble files for')
    parser.add_argument ('-b', '--build', action = 'append', help = 'PicoBB build to assemble files for')
    parser.add_argument ('-t', '--tree', action = 'store', help = 'Folder for examples directory tree')
    parser.add_argument ('-o', '--output', action = 'store', help = 'Name of file to receive LFS image')
    parser.add_argument ('-s', '--size', action = 'store', help = 'Size of LFS image')
    parser.add_argument ('config_file', nargs = '*', help = 'Configuration file, lists programs to include')
    Build (parser.parse_args ())

Run ()
