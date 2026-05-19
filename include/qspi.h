/* qspi.h - Interface to PicoCalc PSRAM using QSPI interface */

#ifndef QSPI_H
#define QSPI_H

void qspi_wait (void);
void qspi_free (void);
bool qspi_cfg_qread (void);
void qspi_qread (uint addr, uint len, uint8_t *buffer);
bool qspi_cfg_qwrite (void);
void qspi_qwrite (uint addr, uint len, uint8_t *buffer);
bool qspi_cfg_sio (void);
void qspi_sread (uint addr, uint len, uint8_t *buffer);
void qspi_swrite (uint addr, uint len, uint8_t *buffer);
void qspi_smode (void);
void qspi_qmode (void);

#endif
