// address.hpp
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <bitset>
#include <concepts>
#include <format>

// C++20 concept to ensure numeric type
template<typename T>
concept NumericType = std::is_integral_v<T>;

class MemoryAddress {
public:
    // Constructor with type-constrained template using concepts
    template<NumericType T>
    explicit MemoryAddress(T address) : address_(static_cast<uint64_t>(address)) {}

    // Get the full address
    [[nodiscard]] uint64_t getAddress() const { return address_; }

    // Extract bit field from address (from position start to end inclusive)
    [[nodiscard]] uint64_t getBits(uint8_t start, uint8_t end) const;
    
    // Get tag bits for a specific cache configuration
    [[nodiscard]] uint64_t getTag(uint8_t blockOffsetBits, uint8_t indexBits) const;
    
    // Get index bits for a specific cache configuration
    [[nodiscard]] uint64_t getIndex(uint8_t blockOffsetBits, uint8_t indexBits) const;
    
    // Get block offset bits
    [[nodiscard]] uint64_t getBlockOffset(uint8_t blockOffsetBits) const;
    
    // String representation
    [[nodiscard]] std::string toString() const;
    
    // Equality operators
    bool operator==(const MemoryAddress& other) const { return address_ == other.address_; }
    bool operator!=(const MemoryAddress& other) const { return address_ != other.address_; }
    
private:
    uint64_t address_;
};

// Hash function for MemoryAddress to use in unordered containers
namespace std {
    template<>
    struct hash<MemoryAddress> {
        std::size_t operator()(const MemoryAddress& address) const {
            return std::hash<uint64_t>()(address.getAddress());
        }
    };
}
