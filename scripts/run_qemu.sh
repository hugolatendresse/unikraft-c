#!/bin/bash
#
# Run the direct block I/O test with QEMU
# Creates a virtual disk if it doesn't exist
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Configuration
DISK_IMG="${DISK_IMG:-$PROJECT_DIR/disk.img}"
DISK_SIZE_MB="${DISK_SIZE_MB:-64}"
KERNEL="${KERNEL:-$PROJECT_DIR/.unikraft/build/blockio-test_qemu-x86_64}"
MEMORY="${MEMORY:-128M}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Direct Block I/O Test Runner ===${NC}"
echo ""

# Create disk image if it doesn't exist
if [ ! -f "$DISK_IMG" ]; then
    echo -e "${YELLOW}Creating ${DISK_SIZE_MB}MB disk image: $DISK_IMG${NC}"
    dd if=/dev/zero of="$DISK_IMG" bs=1M count="$DISK_SIZE_MB" status=progress
    echo ""
fi

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo -e "${RED}Error: Kernel not found at $KERNEL${NC}"
    echo "Please build the project first with: kraft build"
    exit 1
fi

echo "Configuration:"
echo "  Kernel: $KERNEL"
echo "  Disk:   $DISK_IMG ($(du -h "$DISK_IMG" | cut -f1))"
echo "  Memory: $MEMORY"
echo ""

echo -e "${GREEN}Starting QEMU...${NC}"
echo "Press Ctrl+A then X to exit"
echo ""

# Run QEMU with virtio-blk
qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -nographic \
    -m "$MEMORY" \
    -drive file="$DISK_IMG",if=virtio,format=raw \
    -append "console=ttyS0" \
    -cpu host \
    -enable-kvm 2>/dev/null || \
qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -nographic \
    -m "$MEMORY" \
    -drive file="$DISK_IMG",if=virtio,format=raw \
    -append "console=ttyS0"

echo ""
echo -e "${GREEN}QEMU exited${NC}"
