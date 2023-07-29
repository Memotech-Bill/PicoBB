/*
 * Block device in flash
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LFSPICO_H
#define LFSPICO_H

#include <lfs.h>
#include <lfs_util.h>

#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Create a configuration for a flash block device
int ffs_pico_createcfg (struct lfs_config *cfg, int offset, int size);

// Clean up memory associated with block device
int ffs_pico_destroy (const struct lfs_config *cfg);

// Read a block
int ffs_pico_read (const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size);

// Program a block
//
// The block must have previously been erased.
int ffs_pico_prog (const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size);

// Erase a block
//
// A block must be erased before being programmed. The
// state of an erased block is undefined.
int ffs_pico_erase (const struct lfs_config *cfg, lfs_block_t block);

// Sync the block device
int ffs_pico_sync (const struct lfs_config *cfg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
