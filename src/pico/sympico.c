/*  sympico.c -- Symbol table for SDK on the Pico.

	This is free software released under the exact same terms as
	stated in license.txt for the Basic interpreter.  */

int strcmp (const char *ps1, const char *ps2);
void *sympico (char *name);

#ifdef STDIO_USB
static int tud_cdc_connected ();
#else
static int tud_cdc_connected ()
    {
	return 1;
    }
#endif

#ifdef PICO_GUI
#include "sympico_gui.h"
#else
#include "sympico.h"
#endif

void *sympico (char *name)
    {
    int first = 0;
    int last = sizeof(sdkfuncs) / sizeof(symbols) - 1;
    while (first <= last)
        {
        int middle = (first + last) / 2;
        int r = strcmp(name, sdkfuncs[middle].s);
        if (r < 0) last = middle - 1;
        else if (r > 0) first = middle + 1;
        else return sdkfuncs[middle].p;
        }
    return (void *) 0;
    }
