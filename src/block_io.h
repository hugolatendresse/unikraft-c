#ifndef BLOCK_IO_H
#define BLOCK_IO_H

#include <stddef.h>
#include <stdint.h>

// Result structures (same as file_io for consistency)
typedef struct {
    double write_throughput_mbps;
    double read_throughput_mbps;
    double write_latency_us;
    double read_latency_us;
} BlockSequentialResult;

typedef struct {
    double iops;
    double avg_latency_us;
    size_t total_ops;
    size_t read_ops;
    size_t write_ops;
} BlockRandomResult;

// Open block device
int block_io_open(const char* device_path);

// Close block device
void block_io_close(void);

// Check if device is open
int block_io_is_open(void);

// Get device info
size_t block_io_get_block_size(void);
uint64_t block_io_get_total_blocks(void);

// Sequential operations
BlockSequentialResult block_io_sequential(size_t num_blocks);

// Random operations
BlockRandomResult block_io_random(size_t num_ops, int read_heavy);

#endif // BLOCK_IO_H
