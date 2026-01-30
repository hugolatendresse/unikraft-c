#define _GNU_SOURCE
#include "block_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

// O_DIRECT may not be available on all systems
#ifndef O_DIRECT
#define O_DIRECT 0
#endif

// Linux block device ioctls
#ifndef BLKGETSIZE64
#define BLKGETSIZE64 _IOR(0x12, 114, size_t)
#endif
#ifndef BLKSSZGET
#define BLKSSZGET _IO(0x12, 104)
#endif

// Simple LCG random number generator
static uint64_t blk_rng_state = 1;

static void blk_rng_seed(uint64_t seed) {
    blk_rng_state = seed ? seed : 1;
}

static uint32_t blk_rng_next(void) {
    blk_rng_state = blk_rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(blk_rng_state >> 32);
}

static uint64_t blk_rng_next64(uint64_t max) {
    return blk_rng_next() % max;
}

// Get current time in microseconds
static uint64_t blk_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

// Block device state
static int block_fd = -1;
static size_t block_size = 512;
static uint64_t total_blocks = 0;
static uint64_t device_size = 0;

int block_io_open(const char* device_path) {
    if (block_fd >= 0) {
        close(block_fd);
    }
    
    block_fd = open(device_path, O_RDWR | O_DIRECT);
    if (block_fd < 0) {
        // Try without O_DIRECT
        block_fd = open(device_path, O_RDWR);
        if (block_fd < 0) {
            fprintf(stderr, "Failed to open block device %s: errno=%d\n", device_path, errno);
            return -1;
        }
    }
    
    // Get device size
    if (ioctl(block_fd, BLKGETSIZE64, &device_size) < 0) {
        // Try getting file size instead (for regular files)
        off_t size = lseek(block_fd, 0, SEEK_END);
        if (size > 0) {
            device_size = (uint64_t)size;
            lseek(block_fd, 0, SEEK_SET);
        } else {
            fprintf(stderr, "Failed to get device size\n");
            close(block_fd);
            block_fd = -1;
            return -1;
        }
    }
    
    // Get block size
    int sector_size = 0;
    if (ioctl(block_fd, BLKSSZGET, &sector_size) < 0 || sector_size <= 0) {
        sector_size = 512; // Default sector size
    }
    block_size = (size_t)sector_size;
    
    total_blocks = device_size / block_size;
    
    printf("Block device opened: size=%lu bytes, block_size=%zu, total_blocks=%lu\n",
           (unsigned long)device_size, block_size, (unsigned long)total_blocks);
    
    // Seed RNG
    blk_rng_seed(blk_get_time_us());
    
    return 0;
}

void block_io_close(void) {
    if (block_fd >= 0) {
        close(block_fd);
        block_fd = -1;
    }
}

int block_io_is_open(void) {
    return block_fd >= 0;
}

size_t block_io_get_block_size(void) {
    return block_size;
}

uint64_t block_io_get_total_blocks(void) {
    return total_blocks;
}

BlockSequentialResult block_io_sequential(size_t num_blocks) {
    BlockSequentialResult result = {0};
    
    if (block_fd < 0) {
        fprintf(stderr, "Block device not open\n");
        return result;
    }
    
    // Limit to available blocks
    if (num_blocks > total_blocks) {
        num_blocks = total_blocks;
    }
    
    // Allocate buffer
    uint8_t* buffer = (uint8_t*)malloc(block_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return result;
    }
    
    // Fill buffer with pattern
    for (size_t i = 0; i < block_size; i++) {
        buffer[i] = (uint8_t)(i % 256);
    }
    
    // Write test
    lseek(block_fd, 0, SEEK_SET);
    
    uint64_t write_start = blk_get_time_us();
    for (size_t i = 0; i < num_blocks; i++) {
        ssize_t written = write(block_fd, buffer, block_size);
        if (written != (ssize_t)block_size) {
            fprintf(stderr, "Block write failed at block %zu: wrote %zd\n", i, written);
            break;
        }
    }
    fsync(block_fd);
    uint64_t write_end = blk_get_time_us();
    
    // Read test
    lseek(block_fd, 0, SEEK_SET);
    
    uint64_t read_start = blk_get_time_us();
    for (size_t i = 0; i < num_blocks; i++) {
        ssize_t bytes_read = read(block_fd, buffer, block_size);
        if (bytes_read != (ssize_t)block_size) {
            fprintf(stderr, "Block read failed at block %zu: read %zd\n", i, bytes_read);
            break;
        }
    }
    uint64_t read_end = blk_get_time_us();
    
    free(buffer);
    
    // Calculate results
    double total_mb = (num_blocks * block_size) / (1024.0 * 1024.0);
    double write_time_s = (write_end - write_start) / 1000000.0;
    double read_time_s = (read_end - read_start) / 1000000.0;
    
    if (write_time_s > 0) {
        result.write_throughput_mbps = total_mb / write_time_s;
        result.write_latency_us = (write_end - write_start) / (double)num_blocks;
    }
    if (read_time_s > 0) {
        result.read_throughput_mbps = total_mb / read_time_s;
        result.read_latency_us = (read_end - read_start) / (double)num_blocks;
    }
    
    return result;
}

BlockRandomResult block_io_random(size_t num_ops, int read_heavy) {
    BlockRandomResult result = {0};
    
    if (block_fd < 0) {
        fprintf(stderr, "Block device not open\n");
        return result;
    }
    
    // Allocate buffer
    uint8_t* buffer = (uint8_t*)malloc(block_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return result;
    }
    
    // Fill buffer with pattern
    for (size_t i = 0; i < block_size; i++) {
        buffer[i] = (uint8_t)(i % 256);
    }
    
    // Limit operations to available blocks
    uint64_t max_block = total_blocks > 0 ? total_blocks - 1 : 0;
    if (max_block < 1) {
        fprintf(stderr, "Device too small for random I/O\n");
        free(buffer);
        return result;
    }
    
    // Re-seed RNG
    blk_rng_seed(blk_get_time_us());
    
    uint64_t total_time = 0;
    size_t read_ops = 0;
    size_t write_ops = 0;
    int read_threshold = read_heavy ? 80 : 50;
    
    for (size_t i = 0; i < num_ops; i++) {
        uint64_t block_num = blk_rng_next64(max_block + 1);
        int is_read = ((int)blk_rng_next() % 100) < read_threshold;
        off_t offset = (off_t)(block_num * block_size);
        
        uint64_t op_start = blk_get_time_us();
        
        lseek(block_fd, offset, SEEK_SET);
        if (is_read) {
            read(block_fd, buffer, block_size);
            read_ops++;
        } else {
            write(block_fd, buffer, block_size);
            write_ops++;
        }
        
        uint64_t op_end = blk_get_time_us();
        total_time += (op_end - op_start);
    }
    
    free(buffer);
    
    result.total_ops = num_ops;
    result.read_ops = read_ops;
    result.write_ops = write_ops;
    
    double total_time_s = total_time / 1000000.0;
    if (total_time_s > 0) {
        result.iops = num_ops / total_time_s;
        result.avg_latency_us = total_time / (double)num_ops;
    }
    
    return result;
}
