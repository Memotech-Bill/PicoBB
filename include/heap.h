// heap.h - Dynamic memory allocation on BASIC heap

#ifndef HEAP_H
#define HEAP_H

#include <sys/types.h>
#include <stdbool.h>

bool heap_basic ();
bool heap_init (void *base, void *top);
void heap_term (void);
size_t heap_size (void);
void heap_limits (void **base, void **top);
void *heap_malloc (size_t size);
void *heap_calloc (size_t count, size_t size);
void heap_free (void *ptr);

#endif
