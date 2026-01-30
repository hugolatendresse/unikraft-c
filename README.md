# Path 2: Block I/O Benchmark for Unikraft

Tests disk I/O performance to evaluate if Unikraft is suitable for a high-performance DBMS.

## Project Goal

Compare storage access approaches in Unikraft:
1. **VFS + 9P** (host directory mount) - currently working
2. **VFS + virtio-blk** (virtual disk) - driver loads, access in progress
3. **Raw virtio-blk** (direct block access) - future goal

## Quick Start

### First Time Setup

```bash
# Build the unikernel
yes | kraft build --plat qemu --arch x86_64

# Create data directory for 9P mount
mkdir -p data

# Create test disk for virtio-blk (optional)
qemu-img create -f raw test-disk.img 64M
mkfs.ext4 -F test-disk.img
```

### Run the Benchmark

```bash
qemu-system-x86_64 \
    -machine q35 -cpu host -enable-kvm -m 256M -nographic \
    -kernel .unikraft/build/blockio-test_qemu-x86_64 \
    -initrd .unikraft/build/initramfs-x86_64.cpio \
    -drive file=test-disk.img,format=raw,if=virtio \
    -fsdev local,id=myfs,path=./data,security_model=none \
    -device virtio-9p-pci,fsdev=myfs,mount_tag=fs1 \
    -append "console=ttyS0 vfs.fstab=[fs1:/data:9pfs:rw]"
```

Results are printed to the console and saved to `data/results.txt`.

### After Code Changes

```bash
# Rebuild and run
yes | kraft build --plat qemu --arch x86_64

# Then run the QEMU command above
```

## Current Status

### What Works

| Component | Status | Notes |
|-----------|--------|-------|
| Unikraft kernel build | ✅ | Builds from source with virtio-blk |
| virtio-blk driver | ✅ | `Registered blkdev0: virtio-blk` |
| VFS/RamFS | ✅ | Root filesystem works |
| devfs | ✅ | Mounts to /dev |
| 9P filesystem | ✅ | Host directory mounts work |
| Sequential I/O tests | ✅ | Working, see results below |
| Random I/O tests | ✅ | Uses simple LCG random |
| Raw /dev/vda access | ⏳ | Device registered, VFS access TBD |

### Performance Results (RamFS)

Sequential I/O through VFS layer on RamFS (data stored in memory):

| Block Size | Write | Read |
|------------|-------|------|
| 4KB | 7 MB/s | 8,000-16,000 MB/s |
| 64KB | 110 MB/s | 10,000-25,000 MB/s |
| 1MB | 1,000 MB/s | 6,000-16,000 MB/s |

**Note:** Read speeds are very high because data is in RAM. Write speeds reflect VFS/syscall overhead.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              Your Application                    │
│            (C benchmark code)                   │
└───────────────────┬─────────────────────────────┘
                    │ POSIX syscalls
                    ▼
┌─────────────────────────────────────────────────┐
│           Unikraft Syscall Shim                 │
│        (intercepts open/read/write/etc)         │
└───────────────────┬─────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────┐
│              VFS Layer (vfscore)                │
│           (generic filesystem ops)              │
└──────┬────────────────────────┬─────────────────┘
       │                        │
       ▼                        ▼
┌──────────────┐      ┌──────────────────────────┐
│   RamFS      │      │    9P Client (fs1)       │
│ (root: /)    │      │  (mounted at /data)      │
└──────────────┘      └────────────┬─────────────┘
                                   │
                                   ▼
                      ┌──────────────────────────┐
                      │   virtio-9p driver       │
                      └────────────┬─────────────┘
                                   │
                                   ▼
                      ┌──────────────────────────┐
                      │   Host Filesystem        │
                      │   (./data directory)     │
                      └──────────────────────────┘

Also available (not yet fully integrated):
┌──────────────────────────────────────────────────┐
│  virtio-blk driver ──► blkdev0 ──► test-disk.img│
└──────────────────────────────────────────────────┘
```

## Project Structure

```
unikraft-c/
├── Kraftfile              # Unikraft build configuration
├── Dockerfile             # Builds C binary for initramfs
├── src/                   # C benchmark source code
│   ├── main.c
│   ├── benchmark.c/h
│   ├── file_io.c/h
│   └── block_io.c/h
├── data/                  # 9P mount point (created on first run)
├── test-disk.img          # virtio-blk disk image
└── .unikraft/build/       # Built kernel and initramfs
```

## Known Issues

1. **Shutdown assertion** - `Assertion failure: uk_list_empty(&fid_mgmt->fid_active_list)` on exit. This is a 9P cleanup bug, doesn't affect benchmark results.

2. **Raw block device access** - virtio-blk is detected but /dev/vda isn't accessible through VFS yet. Need to either mount a filesystem on it or use raw block device API.

## Building from Source

The Kraftfile configures a native Unikraft build with:
- virtio-blk driver
- VFS layer
- RamFS (root filesystem)
- 9P client (for /data mount)
- devfs (for /dev)
- musl libc

Rebuild with:
```bash
yes | kraft build --plat qemu --arch x86_64
```

## Next Steps

1. **Get /dev/vda working** - Either mount ext4 filesystem on it, or use libukblkdev API directly
2. **Add block device benchmarks** - Compare virtio-blk vs 9P performance  
3. **Profile VFS overhead** - Measure syscall shim + VFS layer costs
