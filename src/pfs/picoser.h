//  picoser.h - Driver routines for serial ports

#ifndef H_BBUART
#define H_BBUART

#include <stdbool.h>
#include <sconfig.h>
#include <pfs.h>

bool uopen (int uid, SERIAL_CONFIG *sc);
void uclose (int uid);
int uread (char *ptr, int size, int nmemb, int uid);
int uwrite (const char *ptr, int size, int nmemb, int uid);
bool parse_sconfig (const char *ps, SERIAL_CONFIG *sc);
struct pfs_pfs *pfs_ser_create (void);

#endif
