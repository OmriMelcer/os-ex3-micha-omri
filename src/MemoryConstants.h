#pragma once

#if __has_include("overrides.h")
#include "overrides.h"
#endif

#include <climits>
#include <stdint.h>

// word
typedef uint32_t word_t;

#define WORD_WIDTH (sizeof(word_t) * CHAR_BIT)

// number of bits in the offset, i.e. the number of bits used to address a word within a page/frame
// Also the number of bits used to address an entry within a page table
#ifndef OFFSET_WIDTH
#define OFFSET_WIDTH 4
#endif
// page/frame size in words
// in this implementation this is also the number of entries in a table
#define PAGE_SIZE (1LL << OFFSET_WIDTH)


// number of bits in a physical address
#ifndef PHYSICAL_ADDRESS_WIDTH
#define PHYSICAL_ADDRESS_WIDTH 10
#endif
// RAM size in words
#define RAM_SIZE (1LL << PHYSICAL_ADDRESS_WIDTH)

// number of bits in a virtual address
#ifndef VIRTUAL_ADDRESS_WIDTH
#define VIRTUAL_ADDRESS_WIDTH 20
#endif
// virtual memory size in words
#define VIRTUAL_MEMORY_SIZE (1LL << VIRTUAL_ADDRESS_WIDTH)

// number of frames in the RAM
#define NUM_FRAMES (RAM_SIZE / PAGE_SIZE)

// number of pages in the virtual memory
#define NUM_PAGES (VIRTUAL_MEMORY_SIZE / PAGE_SIZE)

#define CEIL(VARIABLE) ( (VARIABLE == (int)VARIABLE) ? (int)VARIABLE : (int)VARIABLE+1 )
#define TABLES_DEPTH CEIL((((VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / (double)OFFSET_WIDTH)))

// verify the address width overrides are compatible
static_assert(OFFSET_WIDTH > 0, "OFFSET_WIDTH must be positive");
static_assert(PHYSICAL_ADDRESS_WIDTH > OFFSET_WIDTH, "PHYSICAL_ADDRESS_WIDTH must be greater than OFFSET_WIDTH");
static_assert(VIRTUAL_ADDRESS_WIDTH > OFFSET_WIDTH, "VIRTUAL_ADDRESS_WIDTH must be greater than OFFSET_WIDTH");

// the weights for the evacuation algorithm
#define WEIGHT_EVEN 4
#define WEIGHT_ODD 2
