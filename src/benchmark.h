#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stddef.h>

// Benchmark configuration
typedef struct {
    size_t small_io_size;      // 4KB
    size_t medium_io_size;     // 64KB  
    size_t large_io_size;      // 1MB
    size_t sequential_total;   // Total bytes for sequential test
    size_t random_ops;         // Number of random operations
    const char* data_path;     // Path for filesystem tests
    const char* block_device;  // Path to block device
} BenchmarkConfig;

// Initialize with default config
void benchmark_init(BenchmarkConfig* config);

// Run benchmarks
void benchmark_run_all(const BenchmarkConfig* config);
void benchmark_run_filesystem(const BenchmarkConfig* config);
void benchmark_run_block(const BenchmarkConfig* config);

// Print results to stdout
void benchmark_print_results(void);

#endif // BENCHMARK_H
