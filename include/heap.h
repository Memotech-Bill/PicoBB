// heap.h - Dynamic memory allocation on BASIC heap

#ifndef HEAP_H
#define HEAP_H

#include <sys/types.h>

void heap_init (void);
void heap_clear (void);
void *heap_malloc (size_t size);
void *heap_calloc (size_t count, size_t size);
void heap_free (void *ptr);

#endif
