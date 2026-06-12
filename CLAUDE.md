# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Role

This is a **studying/learning exercise**. The primary goal is to **advise, explain C++ syntax and concepts related to memory management**, and help with complex debugging. Avoid making code edits unless explicitly asked — focus on guidance and explanation.

## Exercise Overview

Implement a virtual memory system using **hierarchical page tables** for OS course (67808, ex3). The student must implement `VirtualMemory.cpp` (the only file to submit, besides README).

- **Root table** is always at frame 0 (never evicted).
- A `0` in any table entry means "page fault" (not mapped / evicted).
- **No global variables or dynamic allocation** — no STL containers (`std::vector`, `std::map`, etc.).
- Do not `#include <iostream>` in submitted files (it defines global variables).

## Build & Run

```bash
# Compile with default constants (from MemoryConstants.h)
g++ src/VirtualMemory.cpp src/PhysicalMemory.cpp tests/test0_sanity.cpp -o test0 -I src
./test0

# Compile with custom memory constants (small address space for debugging)
g++ -DOFFSET_WIDTH=3 -DPHYSICAL_ADDRESS_WIDTH=5 -DVIRTUAL_ADDRESS_WIDTH=6 \
    src/VirtualMemory.cpp src/PhysicalMemory.cpp tests/test0_sanity.cpp -o test0 -I src
# OR using an overrides.h file placed in the same directory as your source:
g++ src/VirtualMemory.cpp src/PhysicalMemory.cpp tests/test0_sanity.cpp -o test0 -I src -I example

# Run the school (reference) solution against any test file
~os/ex/ex3/run_school_solution tests/test0_sanity.cpp
~os/ex/ex3/run_school_solution tests/test0_sanity.cpp example/overrides.h
```

Expected output for `test0_sanity` with default constants:
```
Value at address 0: 0xadded000
Value at address 0xfffff: 0xdeadbeef
```

## Key Constants (MemoryConstants.h)

| Constant | Default | Meaning |
|---|---|---|
| `OFFSET_WIDTH` | 4 | bits for offset within page; also bits per table level |
| `PAGE_SIZE` | 16 | words per page/frame; entries per table |
| `PHYSICAL_ADDRESS_WIDTH` | 10 | → `RAM_SIZE = 1024`, `NUM_FRAMES = 64` |
| `VIRTUAL_ADDRESS_WIDTH` | 20 | → `NUM_PAGES = 65536` |
| `TABLES_DEPTH` | 4 | depth of page table tree (ceiling division) |

`TABLES_DEPTH = ceil((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / OFFSET_WIDTH)`

## Architecture

```
src/
  MemoryConstants.h   — all sizing constants; supports overrides via overrides.h
  PhysicalMemory.h/.cpp — simulated RAM (std::vector) + swap file (unordered_map); DO NOT SUBMIT
  VirtualMemory.h     — API to implement: VMinitialize(), VMread(), VMwrite(), VMgetMapping()
tests/
  test0_sanity.cpp    — basic write/read test
  test0_sanity.txt    — expected output
example/
  overrides.h         — small memory config (OFFSET=3, PHYS=5, VIRT=6) for debugging
  Algorithm Example.pdf
```

## Algorithm Summary

**Address translation**: Split virtual address into `TABLES_DEPTH` indices of `OFFSET_WIDTH` bits each, plus a final offset. Walk the tree using `PMread`/`PMwrite`.

**Page fault handling** — find a free frame by priority:
1. **Empty table frame** (all entries are 0) — reuse it, zero the parent's reference to it.
2. **Unused frame** — `maxFrameSeen + 1 < NUM_FRAMES` means that frame is unallocated.
3. **Eviction** — evict the page with maximum cyclical distance from the page being brought in: `min(NUM_PAGES - |target - p|, |target - p|)`. On ties, pick the lowest page number. Must `PMevict` before reuse and clear the parent's table entry.

**Cyclical distance** formula: `min(NUM_PAGES - abs(pageIn - p), abs(pageIn - p))`

**Key invariants**:
- Frame 0 = root table, always present, never evicted.
- A `0` entry in a table always means "not mapped".
- When allocating a frame for a new **table** (not a leaf page): zero out all `PAGE_SIZE` entries before use.
- When allocating for a **leaf page**: call `PMrestore(frame, pageIndex)`.
- When creating ancestor tables top-down during a page fault, never accidentally evict a frame just allocated.

## Common Pitfalls

- **Signed/unsigned bugs**: Use `uint64_t` consistently; mixing with `int` causes wrong bit shifts.
- **Root table index extraction**: The root table may translate fewer bits than `OFFSET_WIDTH` if `(VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH)` is not divisible by `OFFSET_WIDTH`.
- **Eviction of a just-created frame**: Track all frames allocated in the current walk and exclude them when searching for eviction candidates.
- **Removing parent reference on eviction/empty-table reuse**: Must write `0` back to the parent table entry.
- **`VMgetMapping` is read-only**: must not allocate or restore anything — pure table walk.
