#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>
#include <stdint.h>

// Result structures
typedef struct {
    double write_throughput_mbps;
    double read_throughput_mbps;
    double write_latency_us;
    double read_latency_us;
} SequentialResult;

typedef struct {
    double iops;
    double avg_latency_us;
    size_t total_ops;
    size_t read_ops;
    size_t write_ops;
} RandomResult;

// Initialize file I/O with base path
void file_io_init(const char* base_path);

// Sequential operations
SequentialResult file_io_sequential(size_t block_size, size_t total_bytes);

// Random operations
RandomResult file_io_random(size_t num_ops, size_t block_size, int read_heavy);

#endif // FILE_IO_H
