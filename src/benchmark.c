#include "benchmark.h"
#include "file_io.h"
#include "block_io.h"
#include <stdio.h>
#include <string.h>

void benchmark_init(BenchmarkConfig* config) {
    config->small_io_size = 4 * 1024;           // 4KB
    config->medium_io_size = 64 * 1024;         // 64KB
    config->large_io_size = 1024 * 1024;        // 1MB
    config->sequential_total = 10 * 1024 * 1024; // 10MB
    config->random_ops = 1000;
    config->data_path = "/data";
    config->block_device = "/dev/vda";
}

static void print_sequential_result(const char* name, SequentialResult* result) {
    printf("  %s:\n", name);
    printf("    Write: %.2f MB/s (%.1f us/op)\n", 
           result->write_throughput_mbps, result->write_latency_us);
    printf("    Read:  %.2f MB/s (%.1f us/op)\n",
           result->read_throughput_mbps, result->read_latency_us);
}

static void print_random_result(const char* name, RandomResult* result) {
    printf("  %s:\n", name);
    printf("    IOPS: %.0f\n", result->iops);
    printf("    Avg latency: %.1f us\n", result->avg_latency_us);
    printf("    Ops: %zu total (%zu reads, %zu writes)\n",
           result->total_ops, result->read_ops, result->write_ops);
}

static void print_block_sequential_result(const char* name, BlockSequentialResult* result) {
    printf("  %s:\n", name);
    printf("    Write: %.2f MB/s (%.1f us/block)\n",
           result->write_throughput_mbps, result->write_latency_us);
    printf("    Read:  %.2f MB/s (%.1f us/block)\n",
           result->read_throughput_mbps, result->read_latency_us);
}

static void print_block_random_result(const char* name, BlockRandomResult* result) {
    printf("  %s:\n", name);
    printf("    IOPS: %.0f\n", result->iops);
    printf("    Avg latency: %.1f us\n", result->avg_latency_us);
    printf("    Ops: %zu total (%zu reads, %zu writes)\n",
           result->total_ops, result->read_ops, result->write_ops);
}

void benchmark_run_filesystem(const BenchmarkConfig* config) {
    printf("\n--- Filesystem I/O Tests (VFS + RamFS/9P) ---\n");
    printf("BUILD: C version (no urandom dependency)\n\n");
    
    file_io_init(config->data_path);
    
    printf("Sequential I/O tests:\n\n");
    
    // 4KB sequential
    printf("  Running: Sequential 4KB...");
    fflush(stdout);
    SequentialResult seq_4k = file_io_sequential(config->small_io_size, config->sequential_total);
    printf(" done\n");
    print_sequential_result("Sequential 4KB", &seq_4k);
    printf("\n");
    
    // 64KB sequential
    printf("  Running: Sequential 64KB...");
    fflush(stdout);
    SequentialResult seq_64k = file_io_sequential(config->medium_io_size, config->sequential_total);
    printf(" done\n");
    print_sequential_result("Sequential 64KB", &seq_64k);
    printf("\n");
    
    // 1MB sequential
    printf("  Running: Sequential 1MB...");
    fflush(stdout);
    SequentialResult seq_1m = file_io_sequential(config->large_io_size, config->sequential_total);
    printf(" done\n");
    print_sequential_result("Sequential 1MB", &seq_1m);
    printf("\n");
    
    printf("Random I/O tests:\n\n");
    
    // Random 4KB 50% read
    printf("  Running: Random 4KB (50%% read)...");
    fflush(stdout);
    RandomResult rand_50 = file_io_random(config->random_ops, config->small_io_size, 0);
    printf(" done\n");
    print_random_result("Random 4KB (50% read)", &rand_50);
    printf("\n");
    
    // Random 4KB 80% read
    printf("  Running: Random 4KB (80%% read)...");
    fflush(stdout);
    RandomResult rand_80 = file_io_random(config->random_ops, config->small_io_size, 1);
    printf(" done\n");
    print_random_result("Random 4KB (80% read)", &rand_80);
    printf("\n");
}

void benchmark_run_block(const BenchmarkConfig* config) {
    printf("\n--- Raw Block I/O Tests (virtio-blk) ---\n\n");
    
    if (block_io_open(config->block_device) < 0) {
        printf("Could not open block device %s, skipping block tests\n", config->block_device);
        return;
    }
    
    size_t blk_size = block_io_get_block_size();
    uint64_t total_blks = block_io_get_total_blocks();
    
    printf("Device: %s\n", config->block_device);
    printf("Block size: %zu bytes\n", blk_size);
    printf("Total blocks: %lu\n\n", (unsigned long)total_blks);
    
    printf("Sequential block tests:\n\n");
    
    // Sequential 1000 blocks
    size_t test_blocks = 1000;
    if (test_blocks > total_blks) {
        test_blocks = total_blks;
    }
    
    printf("  Running: Sequential %zu blocks...", test_blocks);
    fflush(stdout);
    BlockSequentialResult seq = block_io_sequential(test_blocks);
    printf(" done\n");
    print_block_sequential_result("Sequential blocks", &seq);
    printf("\n");
    
    printf("Random block tests:\n\n");
    
    // Random 50% read
    printf("  Running: Random blocks (50%% read)...");
    fflush(stdout);
    BlockRandomResult rand_50 = block_io_random(config->random_ops, 0);
    printf(" done\n");
    print_block_random_result("Random blocks (50% read)", &rand_50);
    printf("\n");
    
    // Random 80% read
    printf("  Running: Random blocks (80%% read)...");
    fflush(stdout);
    BlockRandomResult rand_80 = block_io_random(config->random_ops, 1);
    printf(" done\n");
    print_block_random_result("Random blocks (80% read)", &rand_80);
    printf("\n");
    
    block_io_close();
}

void benchmark_run_all(const BenchmarkConfig* config) {
    printf("\n========================================\n");
    printf("  Unikraft Block I/O Benchmark (Path 2)\n");
    printf("  C Version - No /dev/urandom Required\n");
    printf("========================================\n\n");
    
    printf("Configuration:\n");
    printf("  Data path: %s\n", config->data_path);
    printf("  Block device: %s\n", config->block_device);
    printf("  Sequential total: %zu bytes\n", config->sequential_total);
    printf("  Random ops: %zu\n", config->random_ops);
    
    benchmark_run_filesystem(config);
    benchmark_run_block(config);
    
    printf("\n========================================\n");
    printf("  Benchmark Complete\n");
    printf("========================================\n");
}

void benchmark_print_results(void) {
    // Results are printed inline during benchmark_run_*
    // This function is for compatibility with the C++ version
}
