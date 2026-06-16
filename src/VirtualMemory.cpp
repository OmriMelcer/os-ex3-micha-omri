
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

word_t down_the_rabit_hole(uint64_t virtualAdress)
{

  int start_of_first_adress= TABLES_DEPTH * OFFSET_WIDTH;
  uint64_t first_page_index= extract_bits(virtualAdress, start_of_first_adress,
                                          VIRTUAL_ADDRESS_WIDTH - 1);
  word_t next_addr;
  PMread(first_page_index, &next_addr);
  for (int i= 1; i < TABLES_DEPTH; i++)
  {
    uint64_t page_index=
        extract_bits(virtualAdress, OFFSET_WIDTH + i * OFFSET_WIDTH,
                     OFFSET_WIDTH + (i + 1) * OFFSET_WIDTH - 1);
    uint64_t physical_frame_index= VMgetMapping(page_index);
    if (physical_frame_index == 0)
    {
      return 0;
    }
    virtualAdress= physical_frame_index * PAGE_SIZE;
  }
}

int VMread(uint64_t virtualAddress, word_t *value);

int VMwrite(uint64_t virtualAddress, word_t value);

uint64_t VMgetMapping(uint64_t virtualPage);
