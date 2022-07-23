/*  sympico.c -- Symbol table for SDK on the Pico.

	This is free software released under the exact same terms as
	stated in license.txt for the Basic interpreter.  */

#include <string.h>
#include "pico/stdlib.h"

#ifdef STDIO_USB
#include "tusb.h"
#else
static int tud_cdc_connected(){
	return 1;
}
#endif

typedef struct { char *s; void *p; } symbols;
#define SYMADD(X) { #X, &X }
const symbols sdkfuncs[]={
#include "sympico.i"
};

void* sympico(char* name) {
    int first = 0, last = sizeof(sdkfuncs) / sizeof(symbols) - 1;
    if ( strcmp (name, sdkfuncs[first].s) < 0 ) return NULL;
    if ( strcmp (name, sdkfuncs[last].s) > 0 ) return NULL;
    while (last - first  > 1)
        {
        int middle = (first + last) / 2, r = strcmp(name, sdkfuncs[middle].s);
        if (r < 0) last = middle;
        else if (r > 0) first = middle;
        else return sdkfuncs[middle].p;
        }
    return NULL;
}
