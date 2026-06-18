
#include "VirtualMemory.h"
#include "PhysicalMemory.h"

void VMinitialize()
{
  for (uint64_t i= 0; i < PAGE_SIZE; i++)
  {
    PMwrite(i, 0);
  }
}

uint64_t extract_bits(uint64_t num, int start, int end)
{
  // Extract bits from 'num' starting at 'start' to 'end' (inclusive)
  uint64_t mask= ((1ULL << (end - start + 1)) - 1) << start;
  return (num & mask) >> start;
}
int find_empty_table(word_t *frame_index, word_t cur_index)
{
  // 0 for success, 1 for failure
  for (int i= 0; i < PAGE_SIZE; i++)
  {
    word_t res;
    PMread(cur_index * PAGE_SIZE + i, &res);
    if (res != 0)
    {
      int foo= find_empty_table(frame_index, res);
      if (res == 0)
      {
        return 0;
      }
    }
    return 1;
  }
}

word_t page_fault_handler(uint64_t *virtualAddress, word_t prev_addr)
{
  // Handle page fault (e.g., allocate a new physical frame, update page tables,
  // etc.) This is a placeholder implementation and should be replaced with
  // actual logic. For example, you might want to allocate a new physical frame
  // and update the page table entry.
  word_t new_frame_index;
  int res= find_empty_table(&new_frame_index, 0);
}

word_t down_the_rabit_hole(uint64_t virtualAdress)
{

  int start_of_first_adress= TABLES_DEPTH * OFFSET_WIDTH;
  uint64_t first_page_index= extract_bits(virtualAdress, start_of_first_adress,
                                          VIRTUAL_ADDRESS_WIDTH - 1);
  word_t next_addr, tmp, prev_addr;
  PMread(first_page_index, &tmp);
  next_addr= tmp;
  for (int i= 1; i < TABLES_DEPTH - 1; i++)
  {
    if (next_addr == 0)
    {
      page_fault_handler(&virtualAdress, prev_addr);
    }
    uint64_t page_index=
        extract_bits(virtualAdress, OFFSET_WIDTH + i * OFFSET_WIDTH,
                     OFFSET_WIDTH + (i + 1) * OFFSET_WIDTH - 1);
    PMread(next_addr * PAGE_SIZE + page_index, &tmp);
    prev_addr= next_addr * PAGE_SIZE + page_index;
    next_addr= tmp;
  }
  // final stop is the actual phrame of the certain page(given that it exists)
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

  if (virtualAddress < 0 || virtualAddress >= VIRTUAL_MEMORY_SIZE)
  {
    return 1;
  }
  return 0;
}

int VMwrite(uint64_t virtualAddress, word_t value);

uint64_t VMgetMapping(uint64_t virtualPage);
