// picocli.h - Add extensions to OS interface

#ifndef PICOCLI_H
#define PICOCLI_H

#include <stdbool.h>
typedef bool (*CLIFUNC)(const char *);
CLIFUNC add_cli (CLIFUNC new);

#endif
