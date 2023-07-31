/* lfswrap.h - Originally used to supply replacements for stdio functions
   that supported file I/O. The use of the pico-filesystem library now
   means that the NEWLIB supplied functions support file I/O. */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
