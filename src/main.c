/*
 * Direct Block I/O Test
 * Simple write/read/verify test for raw virtio-blk access
 */
#include <uk/print.h>
#include <string.h>
#include <stdint.h>

#include "direct_blk.h"

/* Test pattern byte */
#define TEST_PATTERN 0xAB

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
    int rc;
    void *buf;
    size_t sector_size;
    int success = 1;

    uk_pr_info("\n");
    uk_pr_info("========================================\n");
    uk_pr_info("  Direct Block I/O Test\n");
    uk_pr_info("  Raw virtio-blk access (no VFS)\n");
    uk_pr_info("========================================\n\n");

    /* Initialize block device */
    uk_pr_info("Initializing block device...\n");
    rc = direct_blk_init();
    if (rc < 0) {
        uk_pr_err("Failed to initialize block device: %d\n", rc);
        return 1;
    }

    sector_size = direct_blk_sector_size();
    uk_pr_info("\nTest configuration:\n");
    uk_pr_info("  Sector size: %zu bytes\n", sector_size);
    uk_pr_info("  Total sectors: %zu\n", direct_blk_sector_count());
    uk_pr_info("  I/O alignment: %u bytes\n", direct_blk_ioalign());

    /* Allocate aligned buffer */
    buf = direct_blk_alloc_buf(1);
    if (!buf) {
        uk_pr_err("Failed to allocate I/O buffer\n");
        return 1;
    }

    /* ===== TEST 1: Write pattern to sector 0 ===== */
    uk_pr_info("\n--- Test 1: Write to sector 0 ---\n");
    memset(buf, TEST_PATTERN, sector_size);
    uk_pr_info("Writing pattern 0x%02X to sector 0...\n", TEST_PATTERN);

    rc = direct_blk_write(0, buf, 1);
    if (rc < 0) {
        uk_pr_err("Write failed: %d\n", rc);
        success = 0;
        goto cleanup;
    }
    uk_pr_info("Write successful\n");

    /* ===== TEST 2: Read back and verify ===== */
    uk_pr_info("\n--- Test 2: Read and verify sector 0 ---\n");
    memset(buf, 0x00, sector_size);  /* Clear buffer */
    uk_pr_info("Reading sector 0...\n");

    rc = direct_blk_read(0, buf, 1);
    if (rc < 0) {
        uk_pr_err("Read failed: %d\n", rc);
        success = 0;
        goto cleanup;
    }
    uk_pr_info("Read successful\n");

    /* Verify the data */
    uint8_t *bytes = (uint8_t *)buf;
    int verify_errors = 0;
    for (size_t i = 0; i < sector_size; i++) {
        if (bytes[i] != TEST_PATTERN) {
            if (verify_errors < 5) {
                uk_pr_err("Verification error at byte %zu: expected 0x%02X, got 0x%02X\n",
                          i, TEST_PATTERN, bytes[i]);
            }
            verify_errors++;
        }
    }

    if (verify_errors == 0) {
        uk_pr_info("Verification PASSED: all %zu bytes match pattern\n", sector_size);
    } else {
        uk_pr_err("Verification FAILED: %d byte(s) mismatched\n", verify_errors);
        success = 0;
    }

    /* ===== TEST 3: Write different pattern to sector 1 ===== */
    uk_pr_info("\n--- Test 3: Write to sector 1 ---\n");
    memset(buf, 0xCD, sector_size);
    uk_pr_info("Writing pattern 0xCD to sector 1...\n");

    rc = direct_blk_write(1, buf, 1);
    if (rc < 0) {
        uk_pr_err("Write failed: %d\n", rc);
        success = 0;
        goto cleanup;
    }
    uk_pr_info("Write successful\n");

    /* Read back sector 1 */
    memset(buf, 0x00, sector_size);
    rc = direct_blk_read(1, buf, 1);
    if (rc < 0) {
        uk_pr_err("Read failed: %d\n", rc);
        success = 0;
        goto cleanup;
    }

    verify_errors = 0;
    for (size_t i = 0; i < sector_size; i++) {
        if (bytes[i] != 0xCD) {
            verify_errors++;
        }
    }

    if (verify_errors == 0) {
        uk_pr_info("Verification PASSED for sector 1\n");
    } else {
        uk_pr_err("Verification FAILED for sector 1: %d errors\n", verify_errors);
        success = 0;
    }

    /* ===== TEST 4: Verify sector 0 still has original pattern ===== */
    uk_pr_info("\n--- Test 4: Verify sector 0 unchanged ---\n");
    memset(buf, 0x00, sector_size);
    rc = direct_blk_read(0, buf, 1);
    if (rc < 0) {
        uk_pr_err("Read failed: %d\n", rc);
        success = 0;
        goto cleanup;
    }

    verify_errors = 0;
    for (size_t i = 0; i < sector_size; i++) {
        if (bytes[i] != TEST_PATTERN) {
            verify_errors++;
        }
    }

    if (verify_errors == 0) {
        uk_pr_info("Verification PASSED: sector 0 still contains original pattern\n");
    } else {
        uk_pr_err("Verification FAILED: sector 0 was corrupted\n");
        success = 0;
    }

cleanup:
    direct_blk_free_buf(buf);
    direct_blk_shutdown();

    uk_pr_info("\n========================================\n");
    if (success) {
        uk_pr_info("  ALL TESTS PASSED\n");
    } else {
        uk_pr_info("  SOME TESTS FAILED\n");
    }
    uk_pr_info("========================================\n");

    return success ? 0 : 1;
}
