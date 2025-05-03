#include "address.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits> // Required for numeric_limits

// Define a constant for the bit width of the address type for clarity
constexpr size_t ADDRESS_WIDTH = std::numeric_limits<uint64_t>::digits;

uint64_t MemoryAddress::getBits(uint8_t start, uint8_t end) const {
    // Extract bits from start to end (inclusive)
    if (start > end) {
        std::swap(start, end);
    }

    // Clamp range to the valid bit indices [0, ADDRESS_WIDTH - 1]
    if (end >= ADDRESS_WIDTH) {
        end = ADDRESS_WIDTH - 1;
    }
     if (start >= ADDRESS_WIDTH) {
        return 0; // Start is out of bounds
    }

    // Create a mask for the desired bits
    uint64_t mask = 0;
    for (uint8_t i = start; i <= end; ++i) {
         // Ensure shift amount is less than the width to avoid undefined behavior
         if (i < ADDRESS_WIDTH) {
            mask |= (1ULL << i);
        }
    }

    // Apply the mask and shift the result down to the least significant bit position
    return (address_ & mask) >> start;
}

uint64_t MemoryAddress::getTag(uint8_t blockOffsetBits, uint8_t indexBits) const {
    // Tag bits are the most significant bits after index and offset bits
    uint16_t totalLowerBits = static_cast<uint16_t>(blockOffsetBits) + static_cast<uint16_t>(indexBits);

    // Prevent shifting by the width or more (undefined behavior)
    if (totalLowerBits >= ADDRESS_WIDTH) {
        return 0; // No tag bits remain if offset+index cover the whole address
    }
    // Shift right to remove index and offset bits
    return address_ >> totalLowerBits;
}

uint64_t MemoryAddress::getIndex(uint8_t blockOffsetBits, uint8_t indexBits) const {
    // Index bits are located between the tag and the block offset
    if (indexBits == 0) {
        return 0; // No index bits requested
    }

    // Prevent shifting by the width or more for the offset shift
    if (blockOffsetBits >= ADDRESS_WIDTH) {
        return 0; // Offset covers the entire address, no space for index
    }

    // Shift the address right to remove block offset bits
    uint64_t shifted_address = address_ >> blockOffsetBits;

    // If indexBits cover the remaining width or more, return all remaining bits
    if (indexBits >= ADDRESS_WIDTH) {
         // This condition is technically covered by the mask logic below for indexBits < ADDRESS_WIDTH,
         // but explicit check might be slightly clearer if indexBits could exceed ADDRESS_WIDTH.
         // However, since indexBits is uint8_t, it won't exceed 255.
         // We only need to ensure the mask calculation itself is safe.
         return shifted_address; // Return all bits after the offset shift
    }


    // Create a mask for the index bits (works correctly for 1 <= indexBits < ADDRESS_WIDTH)
    // (1ULL << indexBits) creates a 1 followed by indexBits zeros. Subtracting 1 creates indexBits ones.
    uint64_t mask = (1ULL << indexBits) - 1;

    // Apply the mask to isolate the index bits
    return shifted_address & mask;
}

uint64_t MemoryAddress::getBlockOffset(uint8_t blockOffsetBits) const {
    // Block offset bits are the least significant bits
    if (blockOffsetBits == 0) {
        return 0; // No offset bits requested
    }

    // If requested bits are equal to or exceed the address width, return the whole address
    if (blockOffsetBits >= ADDRESS_WIDTH) {
        return address_;
    }

    // Create a mask for the block offset bits (safe for 1 <= blockOffsetBits < ADDRESS_WIDTH)
    // (1ULL << blockOffsetBits) creates a 1 followed by blockOffsetBits zeros. Subtracting 1 creates blockOffsetBits ones.
    uint64_t mask = (1ULL << blockOffsetBits) - 1;

    // Apply the mask to the address to isolate the lowest bits
    return address_ & mask;
}

std::string MemoryAddress::toString() const {
    // Convert the address to a hexadecimal string representation
    std::stringstream ss;
    // Format as 0x followed by hex digits, padded with leading zeros to match the address width
    ss << "0x" << std::hex << std::setw(ADDRESS_WIDTH / 4) << std::setfill('0') << address_;
    return ss.str();
}

