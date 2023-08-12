// stdio_bt.h - Bluetooth driver for stdio

#ifndef STDIO_BT_H
#define STDIO_BT_H

#include <stdbool.h>
#include <pico/stdio/driver.h>

extern stdio_driver_t stdio_bt;
void stdio_bt_init (void);
bool stdio_bt_connected (void);

#endif
