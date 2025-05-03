// address.cpp
#include "address.hpp"
#include <sstream>
#include <iomanip>

uint64_t MemoryAddress::getBits(uint8_t start, uint8_t end) const {
    // Extract bits from start to end (inclusive)
    if (start > end) {
        std::swap(start, end);
    }
    
    // Create a mask for the desired bits
    uint64_t mask = 0;
    for (uint8_t i = start; i <= end; ++i) {
        mask |= (1ULL << i);
    }
    
    // Apply the mask and shift to the right
    return (address_ & mask) >> start;
}

uint64_t MemoryAddress::getTag(uint8_t blockOffsetBits, uint8_t indexBits) const {
    // Tag bits are the most significant bits after index and offset
    return address_ >> (blockOffsetBits + indexBits);
}

uint64_t MemoryAddress::getIndex(uint8_t blockOffsetBits, uint8_t indexBits) const {
    // Create a mask for the index bits
    uint64_t mask = (1ULL << indexBits) - 1;
    
    // Apply the mask to the address after shifting out the block offset bits
    return (address_ >> blockOffsetBits) & mask;
}

uint64_t MemoryAddress::getBlockOffset(uint8_t blockOffsetBits) const {
    // Create a mask for the block offset bits
    uint64_t mask = (1ULL << blockOffsetBits) - 1;
    
    // Apply the mask to the address
    return address_ & mask;
}

std::string MemoryAddress::toString() const {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << address_;
    return ss.str();
}
