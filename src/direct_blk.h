/*
 * Direct Block I/O via ukblkdev API
 * Bypasses VFS entirely for raw sector access through virtio-blk
 */
#ifndef DIRECT_BLK_H
#define DIRECT_BLK_H

#include <stddef.h>
#include <stdint.h>

/**
 * Initialize the block device subsystem.
 * Must be called before any other direct_blk functions.
 *
 * @return 0 on success, negative error code on failure
 */
int direct_blk_init(void);

/**
 * Shutdown the block device subsystem.
 */
void direct_blk_shutdown(void);

/**
 * Get the sector size in bytes (typically 512).
 */
size_t direct_blk_sector_size(void);

/**
 * Get the total number of sectors on the device.
 */
size_t direct_blk_sector_count(void);

/**
 * Get the required I/O buffer alignment.
 * Buffers passed to read/write must be aligned to this value.
 */
uint16_t direct_blk_ioalign(void);

/**
 * Allocate an I/O buffer with proper alignment.
 * 
 * @param num_sectors Number of sectors the buffer should hold
 * @return Aligned buffer, or NULL on failure. Free with direct_blk_free_buf().
 */
void *direct_blk_alloc_buf(size_t num_sectors);

/**
 * Free a buffer allocated with direct_blk_alloc_buf().
 */
void direct_blk_free_buf(void *buf);

/**
 * Write sectors to the block device.
 *
 * @param start_sector Starting sector number
 * @param buf Source buffer (must be aligned to direct_blk_ioalign())
 * @param num_sectors Number of sectors to write
 * @return 0 on success, negative error code on failure
 */
int direct_blk_write(size_t start_sector, const void *buf, size_t num_sectors);

/**
 * Read sectors from the block device.
 *
 * @param start_sector Starting sector number
 * @param buf Destination buffer (must be aligned to direct_blk_ioalign())
 * @param num_sectors Number of sectors to read
 * @return 0 on success, negative error code on failure
 */
int direct_blk_read(size_t start_sector, void *buf, size_t num_sectors);

#endif /* DIRECT_BLK_H */
