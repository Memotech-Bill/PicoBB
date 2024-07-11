/**
 * @file
 * Dynamic pool memory manager
 *
 * lwIP has dedicated pools for many structures (netconn, protocol control blocks,
 * packet buffers, ...). All these pools are managed here.
 *
 * Custom version for PicoBB - Most items are allocated on BASIC heap
 *
 * @defgroup mempool Memory pools
 * @ingroup infrastructure
 * Custom memory pools

 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Original Author: Adam Dunkels <adam@sics.se>
 *
 * Revised for PicoBB, Memotech Bill, 2024.
 *
 */

#if NET_HEAP
// Force dynamic style variable declarations
#define MEMP_MEM_MALLOC     1
#endif

#include "lwip/opt.h"

#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/stats.h"

#include <string.h>

/* Make sure we include everything we need for size calculation required by memp_std.h */
#include "lwip/pbuf.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/altcp.h"
#include "lwip/ip4_frag.h"
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/priv/api_msg.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/etharp.h"
#include "lwip/igmp.h"
#include "lwip/timeouts.h"
/* needed by default MEMP_NUM_SYS_TIMEOUT */
#include "netif/ppp/ppp_opts.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/priv/nd6_priv.h"
#include "lwip/ip6_frag.h"
#include "lwip/mld6.h"

// Construct struct memp_desc memp_XXX describing size of allocations
#define LWIP_MEMPOOL(name,num,size,desc) LWIP_MEMPOOL_DECLARE(name,num,size,desc)
#include "lwip/priv/memp_std.h"

// Construct memp_pools[] array of pointers to the struct memp_desc for each pool
const struct memp_desc *const memp_pools[MEMP_MAX] = {
#define LWIP_MEMPOOL(name,num,size,desc) &memp_ ## name,
#include "lwip/priv/memp_std.h"
};

#if NET_HEAP
#include "heap.h"

// Static pool of struct sys_timeo since these are allocated from boot time
#define SYS_TIMERS_ALLOC    ((sizeof (struct sys_timeo) + sizeof (uintptr_t) -1) / sizeof (uintptr_t))
uintptr_t   pool_SYS_TMO[MEMP_NUM_SYS_TIMEOUT * SYS_TIMERS_ALLOC];
uintptr_t   *free_SYS_TMO = NULL;

void memp_init (void)
    {
    // Initialise list of free storage for struct systimo
    free_SYS_TMO = NULL;
    for (int i = 0; i < MEMP_NUM_SYS_TIMEOUT; ++i)
        {
        pool_SYS_TMO[SYS_TIMERS_ALLOC * i] = (uintptr_t) free_SYS_TMO;
        free_SYS_TMO = &pool_SYS_TMO[SYS_TIMERS_ALLOC * i];
        }
    // Initialise BASIC heap allocation
    heap_init ();
    }

void *memp_malloc (memp_t type)
    {
    if (type == MEMP_SYS_TIMEOUT)
        {
        // This memory gets allocated when no program is running
        uintptr_t *ptr = free_SYS_TMO;
        free_SYS_TMO = (uintptr_t *) ptr[0];
        return (void *) ptr;
        }
    // Other structures are allocated on BASIC heap
    return heap_malloc (memp_pools[type]->size);
    }

void memp_free (memp_t type, void *mem)
    {
    if (type == MEMP_SYS_TIMEOUT)
        {
        // This memory gets allocated when no program is running
        uintptr_t *ptr = (uintptr_t *) mem;
        ptr[0] = (uintptr_t) free_SYS_TMO;
        free_SYS_TMO = ptr;
        }
    else
        {
        // Other structures are allocated on BASIC heap
        heap_free (mem);
        }
    }

#else   // not NET_HEAP

// Original LWIP code cleaned up by removing conditional compilation blocks
// in order to assist understanding

/**
 * Initialize custom memory pool.
 * Related functions: memp_malloc_pool, memp_free_pool
 *
 * @param desc pool to initialize
 */
void
memp_init_pool(const struct memp_desc *desc)
{
  int i;
  struct memp *memp;

  *desc->tab = NULL;
  memp = (struct memp *)LWIP_MEM_ALIGN(desc->base);
  /* create a linked list of memp elements */
  for (i = 0; i < desc->num; ++i) {
    memp->next = *desc->tab;
    *desc->tab = memp;
    /* cast through void* to get rid of alignment warnings */
    memp = (struct memp *)(void *)((u8_t *)memp + MEMP_SIZE + desc->size
                                  );
  }
}

/**
 * Initializes lwIP built-in pools.
 * Related functions: memp_malloc, memp_free
 *
 * Carves out memp_memory into linked lists for each pool-type.
 */
void
memp_init(void)
{
  u16_t i;

  /* for every pool: */
  for (i = 0; i < LWIP_ARRAYSIZE(memp_pools); i++) {
      printf ("memp_pools[%d] = %p\n", i, memp_pools[i]);
    memp_init_pool(memp_pools[i]);
  }
}

static void *
do_memp_malloc_pool(const struct memp_desc *desc)
{
  struct memp *memp;
  SYS_ARCH_DECL_PROTECT(old_level);

  SYS_ARCH_PROTECT(old_level);

  memp = *desc->tab;

  if (memp != NULL) {

    *desc->tab = memp->next;
    LWIP_ASSERT("memp_malloc: memp properly aligned",
                ((mem_ptr_t)memp % MEM_ALIGNMENT) == 0);
    SYS_ARCH_UNPROTECT(old_level);
    /* cast through u8_t* to get rid of alignment warnings */
    printf ("memp_malloc (%p) = %p\n", desc, (u8_t *)memp + MEMP_SIZE);
    return ((u8_t *)memp + MEMP_SIZE);
  } else {
    SYS_ARCH_UNPROTECT(old_level);
    LWIP_DEBUGF(MEMP_DEBUG | LWIP_DBG_LEVEL_SERIOUS, ("memp_malloc: out of memory in pool %s\n", desc->desc));
  }

  return NULL;
}

/**
 * Get an element from a custom pool.
 *
 * @param desc the pool to get an element from
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
void *
memp_malloc_pool(const struct memp_desc *desc)
{
  LWIP_ASSERT("invalid pool desc", desc != NULL);
  if (desc == NULL) {
    return NULL;
  }

  return do_memp_malloc_pool(desc);
}

/**
 * Get an element from a specific pool.
 *
 * @param type the pool to get an element from
 *
 * @return a pointer to the allocated memory or a NULL pointer on error
 */
void *
memp_malloc(memp_t type)
{
printf ("memp_malloc (%d):\n", type);
  void *memp;
  LWIP_ERROR("memp_malloc: type < MEMP_MAX", (type < MEMP_MAX), return NULL;);

  memp = do_memp_malloc_pool(memp_pools[type]);

  return memp;
}

static void
do_memp_free_pool(const struct memp_desc *desc, void *mem)
{
  printf ("memp_free (%p, %p)\n", desc, mem);
  struct memp *memp;
  SYS_ARCH_DECL_PROTECT(old_level);

  LWIP_ASSERT("memp_free: mem properly aligned",
              ((mem_ptr_t)mem % MEM_ALIGNMENT) == 0);

  /* cast through void* to get rid of alignment warnings */
  memp = (struct memp *)(void *)((u8_t *)mem - MEMP_SIZE);

  SYS_ARCH_PROTECT(old_level);

  memp->next = *desc->tab;
  *desc->tab = memp;

  SYS_ARCH_UNPROTECT(old_level);
}

/**
 * Put a custom pool element back into its pool.
 *
 * @param desc the pool where to put mem
 * @param mem the memp element to free
 */
void
memp_free_pool(const struct memp_desc *desc, void *mem)
{
  LWIP_ASSERT("invalid pool desc", desc != NULL);
  if ((desc == NULL) || (mem == NULL)) {
    return;
  }

  do_memp_free_pool(desc, mem);
}

/**
 * Put an element back into its pool.
 *
 * @param type the pool where to put mem
 * @param mem the memp element to free
 */
void
memp_free(memp_t type, void *mem)
{

  LWIP_ERROR("memp_free: type < MEMP_MAX", (type < MEMP_MAX), return;);

  if (mem == NULL) {
    return;
  }


  do_memp_free_pool(memp_pools[type], mem);

}
#endif  // NET_HEAP
