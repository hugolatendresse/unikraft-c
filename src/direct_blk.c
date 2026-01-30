/*
 * Direct Block I/O via ukblkdev API
 * Bypasses VFS entirely for raw sector access through virtio-blk
 */
#include "direct_blk.h"

#include <uk/blkdev.h>
#include <uk/alloc.h>
#include <uk/print.h>
#include <string.h>
#include <errno.h>

/* Block device state */
static struct uk_blkdev *blkdev = NULL;
static size_t sector_size = 0;
static size_t total_sectors = 0;
static uint16_t io_align = 0;
static int initialized = 0;

/**
 * Queue event callback - called when I/O completes (from interrupt context).
 * This processes completed requests and wakes up any waiting threads.
 */
static void blk_queue_callback(struct uk_blkdev *dev,
                               uint16_t queue_id,
                               void *argp __attribute__((unused)))
{
    /* Process all completed requests in the queue */
    uk_blkdev_queue_finish_reqs(dev, queue_id);
}

int direct_blk_init(void)
{
    struct uk_blkdev_info dev_info;
    struct uk_blkdev_queue_info queue_info;
    struct uk_blkdev_conf dev_conf;
    struct uk_blkdev_queue_conf queue_conf;
    const struct uk_blkdev_cap *cap;
    int rc;

    if (initialized) {
        uk_pr_warn("Block device already initialized\n");
        return 0;
    }

    /* Check how many block devices are available */
    unsigned int num_devs = uk_blkdev_count();
    uk_pr_info("Found %u block device(s)\n", num_devs);

    if (num_devs == 0) {
        uk_pr_err("No block devices found!\n");
        uk_pr_err("Make sure QEMU is started with -drive file=disk.img,if=virtio,format=raw\n");
        return -ENODEV;
    }

    /* Get the first block device */
    blkdev = uk_blkdev_get(0);
    if (!blkdev) {
        uk_pr_err("Failed to get block device 0\n");
        return -ENODEV;
    }

    uk_pr_info("Got block device: %s\n", uk_blkdev_drv_name_get(blkdev));

    /* Query device capabilities */
    rc = uk_blkdev_get_info(blkdev, &dev_info);
    if (rc < 0) {
        uk_pr_err("Failed to get device info: %d\n", rc);
        return rc;
    }
    uk_pr_info("Device supports max %u queue(s)\n", dev_info.max_queues);

    /* Configure device with 1 queue */
    memset(&dev_conf, 0, sizeof(dev_conf));
    dev_conf.nb_queues = 1;

    rc = uk_blkdev_configure(blkdev, &dev_conf);
    if (rc < 0) {
        uk_pr_err("Failed to configure device: %d\n", rc);
        return rc;
    }
    uk_pr_info("Device configured with 1 queue\n");

    /* Query queue capabilities */
    rc = uk_blkdev_queue_get_info(blkdev, 0, &queue_info);
    if (rc < 0) {
        uk_pr_err("Failed to get queue info: %d\n", rc);
        goto err_unconfigure;
    }
    uk_pr_info("Queue 0: min=%u, max=%u descriptors\n",
               queue_info.nb_min, queue_info.nb_max);

    /* Configure queue with callback for interrupt handling */
    memset(&queue_conf, 0, sizeof(queue_conf));
    queue_conf.a = uk_alloc_get_default();
    queue_conf.callback = blk_queue_callback;
    queue_conf.callback_cookie = NULL;

    rc = uk_blkdev_queue_configure(blkdev, 0, queue_info.nb_max, &queue_conf);
    if (rc < 0) {
        uk_pr_err("Failed to configure queue: %d\n", rc);
        goto err_unconfigure;
    }
    uk_pr_info("Queue 0 configured with %u descriptors\n", queue_info.nb_max);

    /* Start the device */
    rc = uk_blkdev_start(blkdev);
    if (rc < 0) {
        uk_pr_err("Failed to start device: %d\n", rc);
        goto err_queue_unconfigure;
    }
    uk_pr_info("Block device started\n");

    /* Get device capabilities (available after start) */
    cap = uk_blkdev_capabilities(blkdev);
    sector_size = cap->ssize;
    total_sectors = cap->sectors;
    io_align = cap->ioalign;

    uk_pr_info("Device capabilities:\n");
    uk_pr_info("  Sector size: %zu bytes\n", sector_size);
    uk_pr_info("  Total sectors: %zu\n", total_sectors);
    uk_pr_info("  Total size: %zu MB\n", (sector_size * total_sectors) / (1024 * 1024));
    uk_pr_info("  I/O alignment: %u bytes\n", io_align);
    uk_pr_info("  Max sectors per request: %zu\n", (size_t)cap->max_sectors_per_req);
    uk_pr_info("  Access mode: %s\n",
               cap->mode == O_RDONLY ? "read-only" :
               cap->mode == O_WRONLY ? "write-only" : "read-write");

    /* Enable interrupts for the queue (required for sync I/O) */
    rc = uk_blkdev_queue_intr_enable(blkdev, 0);
    if (rc < 0) {
        uk_pr_err("Failed to enable queue interrupts: %d\n", rc);
        goto err_stop;
    }
    uk_pr_info("Queue interrupts enabled\n");

    initialized = 1;
    uk_pr_info("Direct block I/O initialized successfully\n");
    return 0;

err_stop:
    uk_blkdev_stop(blkdev);
err_queue_unconfigure:
    uk_blkdev_queue_unconfigure(blkdev, 0);
err_unconfigure:
    uk_blkdev_unconfigure(blkdev);
    blkdev = NULL;
    return rc;
}

void direct_blk_shutdown(void)
{
    if (!initialized || !blkdev)
        return;

    uk_blkdev_queue_intr_disable(blkdev, 0);
    uk_blkdev_stop(blkdev);
    uk_blkdev_queue_unconfigure(blkdev, 0);
    uk_blkdev_unconfigure(blkdev);

    blkdev = NULL;
    initialized = 0;
    uk_pr_info("Block device shutdown complete\n");
}

size_t direct_blk_sector_size(void)
{
    return sector_size;
}

size_t direct_blk_sector_count(void)
{
    return total_sectors;
}

uint16_t direct_blk_ioalign(void)
{
    return io_align;
}

void *direct_blk_alloc_buf(size_t num_sectors)
{
    if (!initialized || sector_size == 0)
        return NULL;

    size_t size = num_sectors * sector_size;
    uint16_t align = io_align > 0 ? io_align : 1;

    return uk_memalign(uk_alloc_get_default(), align, size);
}

void direct_blk_free_buf(void *buf)
{
    if (buf)
        uk_free(uk_alloc_get_default(), buf);
}

int direct_blk_write(size_t start_sector, const void *buf, size_t num_sectors)
{
    int rc;

    if (!initialized) {
        uk_pr_err("Block device not initialized\n");
        return -EINVAL;
    }

    if (start_sector + num_sectors > total_sectors) {
        uk_pr_err("Write beyond device bounds: sector %zu + %zu > %zu\n",
                  start_sector, num_sectors, total_sectors);
        return -EINVAL;
    }

    rc = uk_blkdev_sync_write(blkdev, 0, start_sector, num_sectors, (void *)buf);
    if (rc < 0) {
        uk_pr_err("Write failed at sector %zu: %d\n", start_sector, rc);
    }

    return rc;
}

int direct_blk_read(size_t start_sector, void *buf, size_t num_sectors)
{
    int rc;

    if (!initialized) {
        uk_pr_err("Block device not initialized\n");
        return -EINVAL;
    }

    if (start_sector + num_sectors > total_sectors) {
        uk_pr_err("Read beyond device bounds: sector %zu + %zu > %zu\n",
                  start_sector, num_sectors, total_sectors);
        return -EINVAL;
    }

    rc = uk_blkdev_sync_read(blkdev, 0, start_sector, num_sectors, buf);
    if (rc < 0) {
        uk_pr_err("Read failed at sector %zu: %d\n", start_sector, rc);
    }

    return rc;
}
