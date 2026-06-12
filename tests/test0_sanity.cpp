#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <iostream>
#include <cassert>

int main(int argc, char **argv) {
    VMinitialize();
    auto last_address = VIRTUAL_MEMORY_SIZE - 1;
    
    VMwrite(0, 0xADDED000);
    VMwrite(last_address, 0xDEADBEEF);

    word_t value;
    VMread(0, &value);
    std::cout << "Value at address 0: 0x" << std::hex << value << std::endl;

    VMread(last_address, &value);
    std::cout << "Value at address 0x" << std::hex << last_address << ": 0x" << value << std::endl;

    return 0;
}