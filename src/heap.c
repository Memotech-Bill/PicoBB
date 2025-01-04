// heap.c - Dynamic memory allocation on BASIC heap

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "BBC.h"
#include "heap.h"

void error (int err, const char *msg);

#ifndef DIAG
#define DIAG    0
#endif
#if DIAG
#define DPRINT  printf
#else
#define DPRINT(...)
#endif

typedef struct s_h_mem
    {
    struct s_h_mem  *next;
    size_t          size;
    } H_MEM;

static void *h_base = NULL;
static void *h_top = NULL;
static H_MEM *h_last = NULL;
static bool bBasic = false;

static bool heap_empty (void)
    {
    H_MEM *h_ptr = h_last;
    while (h_ptr != NULL)
        {
        if ((h_ptr->size & 0x80000000) == 0)
            {
            DPRINT ("Buffer size %d active at %p\n", h_ptr->size, h_ptr+1);
            return false;
            }
        h_ptr = h_ptr->next;
        }
    return true;
    }

bool heap_basic ()
    {
    if (bBasic) return true;
    if (! heap_empty ()) return false;
    bBasic = true;
    h_top = NULL;
    h_base = NULL;
    h_last = NULL;
    return true;
    }

bool heap_init (void *base, void *top)
    {
    if ((h_base != NULL) || bBasic)
        {
        if ((base == h_base) || (top == h_top)) return false;
        if (! heap_empty ()) error (0, "Heap busy");
        }
    if (base > top) error (0, "Invalid heap");
    h_base = base;
    h_top = top;
    h_last = NULL;
    bBasic = false;
    return true;
    }

void heap_term (void)
    {
    if (! heap_empty ()) error (0, "Heap busy");
    h_base = NULL;
    h_top = NULL;
    h_last = NULL;
    }

size_t heap_size (void)
    {
    return h_top - h_base;
    }

void heap_limits (void **base, void **top)
    {
    *base = h_base;
    *top = h_top;
    }

void *heap_malloc (size_t size)
    {
    if ((! bBasic) && (h_top == NULL)) error (0, "Heap not initialised");
    size = (size + 7) & -8;
    size |= 0x80000000;
    H_MEM *hptr = h_last;
    while ( hptr != NULL )
        {
        if ( hptr->size == size ) break;
        hptr = hptr->next;
        }
    size &= 0x7FFFFFFF;
    if ( hptr == NULL )
        {
        if ( bBasic )
            {
            heapptr mptr = ((pfree + 3) & -4);
            hptr = (H_MEM *)(zero + mptr);
            mptr += size + sizeof (H_MEM);
            if (mptr > ((void *)esp - zero) - STACK_NEEDED)
                {
                // DPRINT ("pfree = %p, hptr = %p, size = %d, mptr = %p, esp = %p\n", pfree, hptr, size, mptr, esp);
                error (0, NULL); // 'No room'
                }
            pfree = mptr;
            }
        else
            {
            hptr = ((h_last == NULL) ? h_top : h_last) - (size + sizeof (H_MEM));
            if (hptr < (H_MEM *)h_base ) error (0, NULL);
            }
        hptr->next = h_last;
        h_last = hptr;
        }
    hptr->size = size;
    DPRINT ("heap_malloc: size = %d, addr = %p\n", hptr->size, hptr+1);
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
    if (h_last == NULL) return;
    H_MEM *hptr = (H_MEM *) ptr;
    --hptr;
    DPRINT ("heap_free: size = %d, addr = %p\n", hptr->size, hptr+1);
    hptr->size |= 0x80000000;
    }
