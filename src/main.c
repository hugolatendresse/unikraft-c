#include <stdio.h>
#include <string.h>
#include "benchmark.h"

static void print_usage(const char* program) {
    printf("Usage: %s [options]\n\n", program);
    printf("Options:\n");
    printf("  --fs-only       Run only filesystem tests\n");
    printf("  --block-only    Run only block device tests\n");
    printf("  --help          Show this help message\n");
    printf("\nDefault: Run all tests\n");
}

int main(int argc, char* argv[]) {
    BenchmarkConfig config;
    benchmark_init(&config);
    
    int fs_only = 0;
    int block_only = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fs-only") == 0) {
            fs_only = 1;
        } else if (strcmp(argv[i], "--block-only") == 0) {
            block_only = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    printf("\n========================================\n");
    printf("  Unikraft Block I/O Benchmark (Path 2)\n");
    printf("  C Version - No /dev/urandom Required\n");
    printf("========================================\n\n");
    
    printf("System Information:\n");
    printf("  Platform: Unikraft\n");
    printf("  VFS: vfscore\n");
    printf("  Block Driver: virtio-blk\n");
    printf("  Language: C (no libc++ random dependency)\n\n");
    
    printf("Configuration:\n");
    printf("  Data path: %s\n", config.data_path);
    printf("  Block device: %s\n", config.block_device);
    printf("  Sequential total: %zu bytes\n", config.sequential_total);
    printf("  Random ops: %zu\n\n", config.random_ops);
    
    if (fs_only) {
        printf("Running filesystem tests only\n");
        benchmark_run_filesystem(&config);
    } else if (block_only) {
        printf("Running block device tests only\n");
        benchmark_run_block(&config);
    } else {
        printf("Running complete benchmark suite\n");
        benchmark_run_filesystem(&config);
        benchmark_run_block(&config);
    }
    
    printf("\n========================================\n");
    printf("  Benchmark Complete\n");
    printf("========================================\n");
    
    return 0;
}
