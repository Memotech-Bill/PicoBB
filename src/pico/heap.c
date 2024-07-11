// heap.c - Dynamic memory allocation on BASIC heap

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "BBC.h"
#include "heap.h"

typedef struct s_h_mem
    {
    struct s_h_mem  *next;
    size_t          size;
    } H_MEM;

H_MEM *h_top = NULL;

void heap_init (void)
    {
    }

void heap_clear (void)
    {
    h_top = NULL;
    }

void *heap_malloc (size_t size)
    {
    size = (size + 7) & -8;
    size |= 0x80000000;
    H_MEM *hptr = h_top;
    // cyw43_arch_lwip_begin();
    while ( hptr != NULL )
        {
        if ( hptr->size == size ) break;
        hptr = hptr->next;
        }
    size &= 0x7FFFFFFF;
    if ( hptr == NULL )
        {
        unsigned char *mptr = ((pfree + 3) & -4) + (unsigned char *) zero;
        hptr = (H_MEM *) mptr;
        mptr += size + sizeof (H_MEM);
        if (mptr > ((char *)esp - STACK_NEEDED))
            {
            printf ("pfree = %p, hptr = %p, size = %d, mptr = %p, esp = %p\n", pfree, hptr, size, mptr, esp);
            error (0, NULL) ; // 'No room'
            }
        pfree = mptr - (unsigned char *) zero;
        hptr->next = h_top;
        h_top = hptr;
        }
    hptr->size = size;
    // cyw43_arch_lwip_end();
    return (void *)(hptr + 1);
    }

void *heap_calloc (size_t count, size_t size)
    {
    void *pv = heap_malloc (count * size);
    memset (pv, 0, count * size);
    return pv;
    }

void heap_free (void *ptr)
    {
    if (h_top == NULL) return;
    H_MEM *hptr = (H_MEM *) ptr;
    --hptr;
    hptr->size |= 0x80000000;
    }
