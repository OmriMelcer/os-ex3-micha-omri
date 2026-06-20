
#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <algorithm>

void clean_frame(word_t frame)
{
  for (uint64_t i= 0; i < PAGE_SIZE; i++)
  {
    PMwrite(frame * PAGE_SIZE + i, 0);
  }
}

void VMinitialize() { clean_frame(0); }

uint64_t extract_bits(uint64_t num, int start, int end)
{
  // Extract bits from 'num' starting at 'start' to 'end' (inclusive)
  uint64_t mask= ((1ULL << (end - start + 1)) - 1) << start;
  return (num & mask) >> start;
}
int find_empty_table_or_free_frame(word_t &frame_index, word_t cur_index,
                                   int depth, word_t protected_frame,
                                   word_t parent_entry_addr,
                                   word_t &found_parent_entry)
{
  // -1 for all full.
  // 0 for found empty table (frame_index = frame to reuse,
  //    found_parent_entry = address of its OLD parent entry to clear, or
  //    (word_t)-1 for the cold-start root case which has no parent).
  // otherwise: the biggest frame index seen in this subtree.
  int empty_lines= 0;
  if (depth == TABLES_DEPTH)
  {
    return cur_index;
  }
  int largest_frame_so_far= 0;
  for (int i= 0; i < PAGE_SIZE; i++)
  {
    word_t current_memory_context;
    PMread(cur_index * PAGE_SIZE + i, &current_memory_context);
    if (current_memory_context != 0)
    {

      int res= find_empty_table_or_free_frame(
          frame_index, current_memory_context, depth + 1, protected_frame,
          cur_index * PAGE_SIZE + i, found_parent_entry);
      if (res == 0)
      {
        return 0;
      }
      if (res > largest_frame_so_far)
      {
        largest_frame_so_far= res;
      }
      if (current_memory_context > largest_frame_so_far)
      {
        largest_frame_so_far= current_memory_context;
      }
      // We keep the largest frame between what recursive call brings (the
      // largest of all of them, and the actual frames that hold those tables.
      // we want to return the largest frame in the end, so we can use it for
      // the next allocation)
    }
    else
    {
      empty_lines++;
    }
  }
  if (empty_lines == PAGE_SIZE)
  {
    if (cur_index == 0)
    {
      // this is the route table, and cannot be allocated.
      // this means that the complete memory is clean.
      // In this case we will provide 1
      frame_index= 1;
      found_parent_entry= (word_t)-1; // cold start: frame 1 has no parent yet
      return 0;
    }
    if (cur_index != protected_frame)
    {
      frame_index= cur_index;
      found_parent_entry= parent_entry_addr; // clear this before repurposing
      return 0;
    }
    return cur_index;
  }
  if (depth == 0 && largest_frame_so_far + 1 >= NUM_FRAMES)
  {
    // this means that the complete memory is full.
    // Only the top-level call may declare "full": a recursive call must report
    // its subtree's true largest frame, otherwise the parent discards it.
    return -1;
  }
  return largest_frame_so_far;
}
void find_frame_to_evict(uint64_t page_swapped_in, int depth, word_t cur_index,
                         uint64_t cur_page_index, uint64_t prev_page_index,
                         word_t &best_frame_to_evict,
                         uint64_t &best_diff_so_far,
                         uint64_t &evicted_page_index,
                         uint64_t &evicted_page_father)
{
  if (depth == TABLES_DEPTH)
  {
    // cur index is a frame.
    uint64_t diff= std::abs((int64_t)(page_swapped_in - cur_page_index));
    uint64_t cyclic_diff= std::min(diff, (uint64_t)NUM_PAGES - diff);
    if (best_diff_so_far == -1 || cyclic_diff > best_diff_so_far)
    {
      best_diff_so_far= cyclic_diff;
      best_frame_to_evict= cur_index;
      evicted_page_index= cur_page_index;
      evicted_page_father= prev_page_index;
    }
    return;
  }

  for (int i= 0; i < PAGE_SIZE; i++)
  {
    word_t current_memory_context;
    PMread(cur_index * PAGE_SIZE + i, &current_memory_context);
    uint64_t next_page_index= (cur_page_index << OFFSET_WIDTH) | i;
    if (current_memory_context != 0)
    {
      // prev_page_index carries the PHYSICAL ADDRESS of the entry pointing to
      // the child, so the leaf can report its parent entry for clearing.
      find_frame_to_evict(page_swapped_in, depth + 1, current_memory_context,
                          next_page_index, cur_index * PAGE_SIZE + i,
                          best_frame_to_evict, best_diff_so_far,
                          evicted_page_index, evicted_page_father);
    }
  }
}
void insert_table(word_t prev_addr, word_t new_frame_index)
{
  PMwrite(prev_addr, new_frame_index);
  clean_frame(new_frame_index);
}
void restore_leaf(word_t frame_to_evict, word_t prev_addr,
                  uint64_t evicted_page_index, uint64_t page_swapped_in)
{
  PMevict(frame_to_evict, evicted_page_index);
  PMrestore(frame_to_evict, page_swapped_in);
  PMwrite(prev_addr, frame_to_evict);
}

word_t page_fault_handler(uint64_t &virtualAddress, word_t prev_addr,
                          bool is_leaf)
{
  // Handle page fault (e.g., allocate a new physical frame, update page
  // tables, etc.) This is a placeholder implementation and should be replaced
  // with actual logic. For example, you might want to allocate a new physical
  // frame and update the page table entry. input - page. the actual pm memory
  // of the father table. the page that we want to allocate for it. returns
  // the frame_number.
  word_t new_frame_index;
  word_t found_parent_entry= (word_t)-1;
  int res= find_empty_table_or_free_frame(
      new_frame_index, 0, 0, prev_addr / PAGE_SIZE, 0, found_parent_entry);
  if (res == 0)
  {
    // The reused frame may be an empty table that is still referenced by its
    // old parent entry. Clear that reference before repurposing the frame,
    // otherwise the frame ends up with two parents and the tree is corrupted.
    if (found_parent_entry != (word_t)-1)
    {
      PMwrite(found_parent_entry, 0);
    }
    if (!is_leaf)
    {
      insert_table(prev_addr, new_frame_index);
    }
    else
    { // it's clear so nothing to evict
      PMrestore(new_frame_index, virtualAddress >> OFFSET_WIDTH);
      PMwrite(prev_addr, new_frame_index);
    }
    return new_frame_index;
  }
  if (res != -1)
  {
    if (!is_leaf)
    {
      insert_table(prev_addr, res + 1);
    }
    else
    {
      PMrestore(res + 1, virtualAddress >> OFFSET_WIDTH);
      PMwrite(prev_addr, res + 1);
    }
    return res + 1;
  }
  // no free frame, must evict.
  uint64_t page_swapped_in= virtualAddress >> OFFSET_WIDTH;
  word_t frame_to_evict;
  uint64_t best_diff_so_far= -1;
  uint64_t evicted_page_index;
  uint64_t evicted_page_father;
  find_frame_to_evict(page_swapped_in, 0, 0, 0, 0, frame_to_evict,
                      best_diff_so_far, evicted_page_index,
                      evicted_page_father);
  // remove the victim's reference from its parent table before reusing it
  PMwrite(evicted_page_father, 0);
  if (!is_leaf)
  {
    PMevict(frame_to_evict, evicted_page_index);
    insert_table(prev_addr, frame_to_evict);
  }
  else
  {
    restore_leaf(frame_to_evict, prev_addr, evicted_page_index,
                 page_swapped_in);
  }
  return frame_to_evict;
}

word_t down_the_rabit_hole(uint64_t virtualAdress, bool use_page_fault_handler)
{

  int start_of_first_adress= TABLES_DEPTH * OFFSET_WIDTH;
  uint64_t first_page_index= extract_bits(virtualAdress, start_of_first_adress,
                                          VIRTUAL_ADDRESS_WIDTH - 1);
  word_t next_addr, tmp, prev_addr= first_page_index;
  PMread(first_page_index, &tmp);
  next_addr= tmp;
  for (int i= TABLES_DEPTH - 1; i != 0; i--)
  {
    if (next_addr == 0)
    {
      if (!use_page_fault_handler)
      {
        return 0;
      }
      next_addr= page_fault_handler(virtualAdress, prev_addr, false);
    }
    uint64_t page_index= extract_bits(virtualAdress, i * OFFSET_WIDTH,
                                      (i + 1) * OFFSET_WIDTH - 1);
    PMread(next_addr * PAGE_SIZE + page_index, &tmp);
    prev_addr= next_addr * PAGE_SIZE + page_index;
    next_addr= tmp;
  }
  // final stop is the actual frame of the certain page(given that it exists)
  if (next_addr == 0)
  {
    if (!use_page_fault_handler)
    {
      return 0;
    }
    next_addr= page_fault_handler(virtualAdress, prev_addr, true);
  }
  return next_addr;
}

int VMread(uint64_t virtualAddress, word_t *value)
{
  /**
   * Read algorithm:
   * 1. receive a page. in VM format. (that what was allocated).
   * 2. if finish DFS and found the page, return the value.
   * 3. if encountered a 0 on the way - page fault, assume all memories are
   * readable from disk even untouched. fault handler.
   *  */

  if (virtualAddress >= VIRTUAL_MEMORY_SIZE || value == nullptr)
  {
    return 0;
  }
  PMread(down_the_rabit_hole(virtualAddress, true) * PAGE_SIZE +
             extract_bits(virtualAddress, 0, OFFSET_WIDTH - 1),
         value);
  return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
  {
    return 0;
  }
  PMwrite(down_the_rabit_hole(virtualAddress, true) * PAGE_SIZE +
              extract_bits(virtualAddress, 0, OFFSET_WIDTH - 1),
          value);
  return 1;
}

uint64_t VMgetMapping(uint64_t virtualPage)
{
  return down_the_rabit_hole((virtualPage << OFFSET_WIDTH), false);
}
