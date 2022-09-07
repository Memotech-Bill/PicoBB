//  zmodem.h - ZModem implementation

#ifndef ZMODEM_H
#define ZMODEM_H

#define ZDIAG   1

#if ZDIAG
void zdiag (const char *psFmt,...);
#endif
void zreceive (const char *pcmd);
void zsend (const char *pcmd);
void ymodem (int mode);

#endif
