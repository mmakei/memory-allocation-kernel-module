# Memory Allocation Kernel Module

## Overview
This Linux kernel module (`/dev/memalloc`) provides an interface for allocating and freeing pages of memory from user space via custom IOCTL commands.  
It supports configurable read-only or read-write permissions per page and ensures proper page table setup and TLB flushing for correctness.  
part of **CSE 330 – Operating Systems** coursework

---

## Features
- Allocate up to 4096 pages of memory with specific read/write permissions.
- `ALLOCATE` command maps zeroed pages into user virtual address space.
- `FREE` command unmaps and deallocates pages.
- Manual page table creation from PGD → PTE.
- Proper TLB flushing after mapping changes.
- Error handling for already mapped pages and allocation limits.

---

## Technologies Used
- **Language:** C  
- **Platform:** Linux Kernel 6.10  
- **Concepts:** Page table management, IOCTL interface, virtual-to-physical mapping, permission flags, TLB maintenance

---

## How It Works
1. User program issues `ALLOCATE` or `FREE` via ioctl.
2. For `ALLOCATE`:
   - Pages are allocated with `get_zeroed_page()`.
   - Page table entries are created with custom helper functions.
   - Permissions are set to either read-only or read-write.
3. For `FREE`:
   - Page table entry is cleared.
   - Page is freed and TLB is flushed.
4. Device node `/dev/memalloc` is used for interaction.

---

## Build & Run
```bash
# 1. Build the Module
make

# 2. Insert the Module
sudo insmod memalloc.ko

# 3. Create Device Node (replace <major> with number from dmesg)
sudo mknod /dev/memalloc c <major> 0

# 4. Remove the Module
sudo rmmod memalloc

