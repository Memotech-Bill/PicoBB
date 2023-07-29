/*
 * Block device in flash modified by Eric Olson from lfs_rambd.c
 * Clean-up for generic use - Memotech-Bill.
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lfs.h>
#include <hardware/flash.h>
#ifdef PICO_MCLOCK
#include "pico/multicore.h"
#endif

int ffs_pico_read (const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int ffs_pico_prog (const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int ffs_pico_erase (const struct lfs_config *cfg, lfs_block_t block);
int ffs_pico_sync (const struct lfs_config *cfg);

int ffs_pico_createcfg (struct lfs_config *cfg, int offset, int size)
    {
    if ( offset % FLASH_PAGE_SIZE != 0 ) return -1;
    memset (cfg, 0, sizeof (struct lfs_config));
    cfg->context = (void *) (XIP_BASE + offset);
    cfg->read = ffs_pico_read;
    cfg->prog = ffs_pico_prog;
    cfg->erase = ffs_pico_erase;
    cfg->sync = ffs_pico_sync;
    cfg->read_size = 1;
    cfg->prog_size = FLASH_PAGE_SIZE;
    cfg->block_size = FLASH_SECTOR_SIZE;
    cfg->block_count = size / FLASH_SECTOR_SIZE;
    cfg->cache_size = FLASH_PAGE_SIZE;
    cfg->cache_size = 256;
    cfg->lookahead_size = 32;
    cfg->block_cycles = 256;
	return 0;
    }

int ffs_pico_destroy (const struct lfs_config *cfg)
    {
	return 0;
    }

int ffs_pico_read (const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
    {
    uint8_t *ffs_mem  = (uint8_t *) cfg->context;

	// check if read is valid
	LFS_ASSERT (off  % cfg->read_size == 0);
	LFS_ASSERT (size % cfg->read_size == 0);
	LFS_ASSERT (block < cfg->block_count);

	// read data
	memcpy (buffer, &ffs_mem[block*cfg->block_size + off], size);

	return 0;
    }

int ffs_pico_prog (const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
    {
    uint8_t *ffs_mem  = (uint8_t *) cfg->context;

	// check if write is valid
	LFS_ASSERT (off  % cfg->prog_size == 0);
	LFS_ASSERT (size % cfg->prog_size == 0);
	LFS_ASSERT (block < cfg->block_count);

	// program data
#if defined (PICO_MCLOCK)
    multicore_lockout_start_blocking ();
#endif
	uint32_t ints = save_and_disable_interrupts ();
	flash_range_program (&ffs_mem[block*cfg->block_size + off]
		-(uint8_t *)XIP_BASE, buffer, size);
	restore_interrupts (ints);
#if defined (PICO_MCLOCK)
    multicore_lockout_end_blocking ();
#endif

	return 0;
    }

int ffs_pico_erase (const struct lfs_config *cfg, lfs_block_t block)
    {
    uint8_t *ffs_mem  = (uint8_t *) cfg->context;

	// check if erase is valid
	LFS_ASSERT (block < cfg->block_count);

#if defined (PICO_MCLOCK)
    multicore_lockout_start_blocking ();
#endif
	uint32_t ints = save_and_disable_interrupts ();
	flash_range_erase (&ffs_mem[block*cfg->block_size]
		-(uint8_t *)XIP_BASE, cfg->block_size);
	restore_interrupts (ints);
#if defined (PICO_MCLOCK)
    multicore_lockout_end_blocking ();
#endif

	return 0;
    }

int ffs_pico_sync (const struct lfs_config *cfg)
    {
	// sync does nothing because we aren't backed by anything real
	return 0;
    }
