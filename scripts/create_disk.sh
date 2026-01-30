#!/bin/bash
# Creates a virtio-blk disk image for block device testing
#
# NOTE: This disk image CANNOT be used with base:latest runtime.
# It's here for future use when we configure virtio-blk support.
#
# Usage: ./create_disk.sh [size] [filename]
# Example: ./create_disk.sh 256M mydisk.img

set -e

DISK_SIZE="${1:-256M}"
DISK_IMAGE="${2:-blockio-disk.img}"

echo "Creating disk image: $DISK_IMAGE ($DISK_SIZE)"
echo ""
echo "WARNING: This disk cannot be used with 'kraft run --disk' on base:latest."
echo "         The base:latest runtime doesn't expose /dev/vda."
echo ""

qemu-img create -f raw "$DISK_IMAGE" "$DISK_SIZE"
mkfs.ext4 -F "$DISK_IMAGE"

echo ""
echo "Disk created: $DISK_IMAGE"
echo ""
echo "To use this, you would need to:"
echo "  1. Build Unikraft from source with virtio-blk support, OR"
echo "  2. Find a runtime that exposes /dev/vda"
