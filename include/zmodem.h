//  zmodem.h - ZModem implementation

#ifndef ZMODEM_H
#define ZMODEM_H

#define ZDIAG   1

#if ZDIAG
void zdiag (const char *psFmt,...);
#endif
void zmodem (const char *phex);

#endif
