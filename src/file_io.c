#include "file_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

// Simple LCG random number generator (no /dev/urandom needed)
static uint64_t rng_state = 1;

static void rng_seed(uint64_t seed) {
    rng_state = seed ? seed : 1;
}

static uint32_t rng_next(void) {
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(rng_state >> 32);
}

static uint32_t rng_next_max(uint32_t max) {
    return rng_next() % max;
}

// Get current time in microseconds
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

// Base path for file operations
static char base_path[256] = "/data";

void file_io_init(const char* path) {
    if (path) {
        strncpy(base_path, path, sizeof(base_path) - 1);
        base_path[sizeof(base_path) - 1] = '\0';
    }
    
    // Seed RNG with current time
    rng_seed(get_time_us());
}

static void get_full_path(char* out, size_t out_size, const char* filename) {
    snprintf(out, out_size, "%s/%s", base_path, filename);
}

SequentialResult file_io_sequential(size_t block_size, size_t total_bytes) {
    SequentialResult result = {0};
    char path[512];
    get_full_path(path, sizeof(path), "seq_test.dat");
    
    // Allocate buffer and fill with pattern
    uint8_t* buffer = (uint8_t*)malloc(block_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return result;
    }
    
    for (size_t i = 0; i < block_size; i++) {
        buffer[i] = (uint8_t)(i % 256);
    }
    
    size_t num_ops = total_bytes / block_size;
    
    // Write test
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file for writing: %s (errno=%d)\n", path, errno);
        free(buffer);
        return result;
    }
    
    uint64_t write_start = get_time_us();
    for (size_t i = 0; i < num_ops; i++) {
        ssize_t written = write(fd, buffer, block_size);
        if (written != (ssize_t)block_size) {
            fprintf(stderr, "Write failed at op %zu\n", i);
            break;
        }
    }
    fsync(fd);
    uint64_t write_end = get_time_us();
    
    // Read test
    lseek(fd, 0, SEEK_SET);
    
    uint64_t read_start = get_time_us();
    for (size_t i = 0; i < num_ops; i++) {
        ssize_t bytes_read = read(fd, buffer, block_size);
        if (bytes_read != (ssize_t)block_size) {
            fprintf(stderr, "Read failed at op %zu\n", i);
            break;
        }
    }
    uint64_t read_end = get_time_us();
    
    close(fd);
    unlink(path);
    free(buffer);
    
    // Calculate results
    double write_time_s = (write_end - write_start) / 1000000.0;
    double read_time_s = (read_end - read_start) / 1000000.0;
    double total_mb = total_bytes / (1024.0 * 1024.0);
    
    if (write_time_s > 0) {
        result.write_throughput_mbps = total_mb / write_time_s;
        result.write_latency_us = (write_end - write_start) / (double)num_ops;
    }
    if (read_time_s > 0) {
        result.read_throughput_mbps = total_mb / read_time_s;
        result.read_latency_us = (read_end - read_start) / (double)num_ops;
    }
    
    return result;
}

RandomResult file_io_random(size_t num_ops, size_t block_size, int read_heavy) {
    RandomResult result = {0};
    char path[512];
    get_full_path(path, sizeof(path), "random_test.dat");
    
    // Allocate buffer
    uint8_t* buffer = (uint8_t*)malloc(block_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return result;
    }
    
    for (size_t i = 0; i < block_size; i++) {
        buffer[i] = (uint8_t)(i % 256);
    }
    
    // Pre-create file with sufficient size
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "Failed to create file for random I/O: %s\n", path);
        free(buffer);
        return result;
    }
    
    // Write initial data
    for (size_t i = 0; i < num_ops; i++) {
        write(fd, buffer, block_size);
    }
    fsync(fd);
    lseek(fd, 0, SEEK_SET);
    
    // Re-seed RNG for reproducibility
    rng_seed(get_time_us());
    
    uint64_t total_time = 0;
    size_t read_ops = 0;
    size_t write_ops = 0;
    
    // Read/write ratio: read_heavy=1 means 80% reads, =0 means 50% reads
    int read_threshold = read_heavy ? 80 : 50;
    
    for (size_t i = 0; i < num_ops; i++) {
        off_t offset = (off_t)(rng_next_max(num_ops) * block_size);
        int is_read = (int)(rng_next_max(100)) < read_threshold;
        
        uint64_t op_start = get_time_us();
        
        lseek(fd, offset, SEEK_SET);
        if (is_read) {
            read(fd, buffer, block_size);
            read_ops++;
        } else {
            write(fd, buffer, block_size);
            write_ops++;
        }
        
        uint64_t op_end = get_time_us();
        total_time += (op_end - op_start);
    }
    
    close(fd);
    unlink(path);
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
