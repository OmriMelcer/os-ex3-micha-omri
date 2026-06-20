# VirtualMemory.cpp — Debugging Handoff

Snapshot of the eviction/allocation bugs found while stress-testing. The walk
(translation) and the simple cases are correct; everything below only triggers
when **RAM fills up in a multi-level tree** (so `test0_sanity` and shallow
configs pass and hide it).

## Current state of the real file

Fixed and in `src/VirtualMemory.cpp`:
- Address-walk fixes (bit-slice order, leaf `is_leaf=true`, re-read `prev_addr`, `prev_addr` init).
- `find_frame_to_evict` returns over the whole leaf case.
- `protected_frame` exclusion in empty-table reuse (cold-start self-reference).
- **#2 — eviction clears the victim's parent entry** (`evicted_page_father`):
  recursion passes the entry address `cur_index * PAGE_SIZE + i`; handler does
  `PMwrite(evicted_page_father, 0)` before reuse.

NOT applied to the real file (only tested in `/tmp` scratch copies): Bug A fix,
Bug B, VMgetMapping shift.

## Test harness used

`/tmp/evict_test.cpp` (writes every word, reads back, checks) — recreate if gone:

```cpp
#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cstdio>
int main() {
    VMinitialize();
    uint64_t n = VIRTUAL_MEMORY_SIZE;
    for (uint64_t a = 0; a < n; ++a) VMwrite(a, (word_t)(a * 7 + 1));
    int errors = 0;
    for (uint64_t a = 0; a < n; ++a) {
        word_t v; VMread(a, &v);
        if (v != (word_t)(a * 7 + 1)) { if (errors < 10) printf("MISMATCH @%llu got %u exp %u\n",(unsigned long long)a,v,(word_t)(a*7+1)); errors++; }
    }
    printf("NUM_FRAMES=%llu NUM_PAGES=%llu TABLES_DEPTH=%d errors=%d\n",
        (unsigned long long)NUM_FRAMES,(unsigned long long)NUM_PAGES,(int)TABLES_DEPTH,errors);
    return errors ? 1 : 0;
}
```

Build pattern (run from repo root):
```bash
g++ -g -DOFFSET_WIDTH=2 -DPHYSICAL_ADDRESS_WIDTH=6 -DVIRTUAL_ADDRESS_WIDTH=10 \
    src/VirtualMemory.cpp src/PhysicalMemory.cpp /tmp/evict_test.cpp -o /tmp/ef -I src
/tmp/ef
```
NOTE: zsh does NOT word-split unquoted vars — don't put multiple `-D` flags in a
shell variable; write them literally on the command line.

## What passes

| Config | Result |
|---|---|
| default (`test0_sanity`) | ✅ `0xadded000` / `0xdeadbeef` |
| shallow `OFFSET=3,PHYS=5,VIRT=6` (4 frames, 8 pages, TABLES_DEPTH=1) | ✅ 0 errors — exercises eviction + #2 parent-clear |

## What fails

| Config | Original | After Bug A fix |
|---|---|---|
| deep `OFFSET=2,PHYS=6,VIRT=10` (16 frames, 256 pages, TABLES_DEPTH=4) | `PMwrite physicalAddress < RAM_SIZE` (PhysicalMemory.cpp:30) | `PMevict swapFile.find(...) == end()` (PhysicalMemory.cpp:41) |
| `PHYSICAL_ADDRESS_WIDTH=8` (16 frames, 65536 pages) | — | `PMevict swapFile.find(...) == end()` |

Eviction (`res == -1`) is never even reached before the original crash → the
original crash is in the **allocation/free-frame** path, not eviction.

---

## BUG A — `find_empty_table_or_free_frame` leaks `-1` out of the recursion

**Location:** `find_empty_table_or_free_frame`, the `if (largest_frame_so_far + 1 >= NUM_FRAMES) return -1;` block.

**Symptom:** `PMwrite physicalAddress < RAM_SIZE` — an out-of-range frame index
is used (frame ≥ NUM_FRAMES), i.e. an already-in-use frame gets handed out.

**Evidence (RAM dump at the failing `find_empty` top-level call, deep config):**
```
[find_empty depth0] largest=1 protected=15 | RAM:
 f0[1,0,0,0] f1[2,15,0,0] f2[3,8,13,14] f3[4,5,6,7] f4[...] ... f13[14,15,14,14] f14[15,15,15,15] f15[0,0,0,0]
```
All 16 frames are in use, but `find_empty` returned `largest=1`.

**Root cause (hand-trace of the dump):**
- `frame2`'s subtree internally reaches frame 15, so `largest=15` there →
  `15 + 1 >= 16` → `frame2` **returns `-1`**.
- In `frame1`: `res = -1`; the line `if (res > largest_frame_so_far)` is
  `-1 > 0` → false → **the subtree's real max (15) is discarded**. `frame1`
  also ends up `largest=15` → returns `-1`.
- At root: `res = -1`, discarded again; root falls back to
  `current_memory_context = 1` → returns `largest = 1`.
- Caller takes `res + 1 = 2`, a frame that is already in use → corruption →
  later out-of-range access.

`-1` must mean "the WHOLE memory is full", but it was being produced by any
subtree whose frames happen to include `NUM_FRAMES-1`.

**Fix (verified to remove the PMwrite crash):** only the top-level call may
declare "full":
```cpp
if (depth == 0 && largest_frame_so_far + 1 >= NUM_FRAMES)
    return -1;
return largest_frame_so_far;
```
Recursive calls (depth > 0) then always return their subtree's true largest
frame; only `page_fault_handler`'s entry call (depth 0) decides eviction.

**Status:** fix known + verified in `/tmp/srcfix`. Necessary but NOT sufficient
(deep config then hits Bug B). Not yet applied to the real file.

---

## BUG B — deep-tree `PMevict` double-evict (still present after fixing A)

**Location:** eviction path (`find_frame_to_evict` selection +
`page_fault_handler` eviction branch). Not root-caused yet.

**Symptom:** `assert(swapFile.find(evictedPageIndex) == swapFile.end())` fails in
`PMevict` (PhysicalMemory.cpp:41). Since `PMrestore` erases the swap entry
(PhysicalMemory.cpp:61), evicting a page already in swap means **the same page
is treated as resident in two frames** → tree corruption.

**Reproduce:** deep config above, with Bug A's fix applied.

**Leading hypothesis (unconfirmed):** the CLAUDE.md pitfall —
> *"Eviction of a just-created frame: track all frames allocated in the current
> walk and exclude them when searching for eviction candidates."*

A single `VMwrite` that faults in several table levels calls the fault handler
multiple times while memory is full. Candidate causes to check:
- `find_frame_to_evict` not excluding frames allocated earlier in **this** walk.
- `evicted_page_index` reconstruction colliding once the tree is corrupted
  (two parents → one frame yields duplicate reconstructed page numbers).
- selecting/evicting a frame that is on the current translation path.

**Suggested next steps for the session:**
1. Apply Bug A's fix first (otherwise corruption masks B).
2. Re-run deep config; instrument `page_fault_handler` to log every
   `PMevict(frame, page)` and assert `page` not already in `swapFile` *before*
   the call, dumping the path of frames allocated in the current `VMwrite`.
3. Check whether the evicted `frame`/`page` belongs to the current walk path.

---

## Also open (unrelated to A/B)

**VMgetMapping** passes `virtualPage` straight into `down_the_rabit_hole`, which
treats its argument as a full virtual address (bottom `OFFSET_WIDTH` bits =
offset). Pass `virtualPage << OFFSET_WIDTH` so the page bits land on the table
indices. Example (default cfg): page `0x1234` as-is walks indices `0,1,2,3`
(page `0x0123`) instead of `1,2,3,4`.
