// address_test.cpp
#include "address.hpp"
#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <limits>

// Test framework macros
#define TEST_CASE(name) do { \
std::cout << "Running test: " << name << "... "; \
    bool passed = true; \
try {

#define END_TEST \
} catch (const std::exception& e) { \
        std::cout << "FAILED (" << e.what() << ")" << std::endl; \
        passed = false; \
} \
    if (passed) { \
        std::cout << "PASSED" << std::endl; \
} \
} while(0)

#define ASSERT(condition) \
    if (!(condition)) { \
            std::cerr << "Assertion failed: " << #condition << " at line " << __LINE__ << std::endl; \
            passed = false; \
            return; \
    }

// Test 1: Basic construction with different numeric types
void testConstructorWithDifferentTypes() {
    TEST_CASE("Constructor with different numeric types") {
        // Test with different integral types
        MemoryAddress addr1(static_cast<int32_t>(0x12345678));
        ASSERT(addr1.getAddress() == 0x12345678);

        MemoryAddress addr2(static_cast<uint32_t>(0x87654321));
        ASSERT(addr2.getAddress() == 0x87654321);

        MemoryAddress addr3(static_cast<int64_t>(0x123456789ABCDEF0));
        ASSERT(addr3.getAddress() == 0x123456789ABCDEF0);

        MemoryAddress addr4(static_cast<uint64_t>(0xFEDCBA0987654321));
        ASSERT(addr4.getAddress() == 0xFEDCBA0987654321);

        // Edge cases
        MemoryAddress addr5(static_cast<uint8_t>(0xFF));
        ASSERT(addr5.getAddress() == 0xFF);

        MemoryAddress addr6(static_cast<int8_t>(-1));
        ASSERT(addr6.getAddress() == 0xFFFFFFFFFFFFFFFF); // Sign extension

        // Zero address
        MemoryAddress addr7(0);
        ASSERT(addr7.getAddress() == 0);

        // Maximum values
        MemoryAddress addr8(std::numeric_limits<uint64_t>::max());
        ASSERT(addr8.getAddress() == std::numeric_limits<uint64_t>::max());
    }
    END_TEST;
}

// Test 2: Test getBits functionality
void testGetBits() {
    TEST_CASE("getBits extraction") {
        // Test address: 0x123456789ABCDEF0
        MemoryAddress addr(0x123456789ABCDEF0);

        // Single bit extraction
        ASSERT(addr.getBits(0, 0) == 0);  // LSB
        ASSERT(addr.getBits(4, 4) == 1);  // Bit 4

        // Byte extraction
        ASSERT(addr.getBits(0, 7) == 0xF0);   // Lowest byte
        ASSERT(addr.getBits(8, 15) == 0xDE);  // Second byte
        ASSERT(addr.getBits(16, 23) == 0xBC); // Third byte

        // Multi-byte extraction
        ASSERT(addr.getBits(0, 15) == 0xDEF0);   // Two bytes
        ASSERT(addr.getBits(32, 47) == 0x5678);  // Two bytes from middle

        // Edge cases
        ASSERT(addr.getBits(59, 59) == 0);  // High bit
        ASSERT(addr.getBits(0, 63) == 0x123456789ABCDEF0); // Entire address

        // Test with reversed indices (should handle internally)
        ASSERT(addr.getBits(15, 0) == 0xDEF0);  // Same as (0, 15)

        // Test with address containing all 1s
        MemoryAddress allOnes(0xFFFFFFFFFFFFFFFF);
        ASSERT(allOnes.getBits(0, 7) == 0xFF);
        ASSERT(allOnes.getBits(32, 47) == 0xFFFF);

        // Test with address containing all 0s
        MemoryAddress allZeros(0);
        ASSERT(allZeros.getBits(0, 7) == 0);
        ASSERT(allZeros.getBits(32, 47) == 0);
    }
    END_TEST;
}

// Test 3: Test cache field extraction (tag, index, offset)
void testCacheFieldExtraction() {
    TEST_CASE("Cache field extraction") {
        // Configuration 1: 6-bit block offset, 8-bit index
        uint8_t blockOffsetBits1 = 6;
        uint8_t indexBits1 = 8;

        // Test address: 0x123456789ABCDEF0
        MemoryAddress addr1(0x123456789ABCDEF0);

        // Bits 0-5: block offset = 0x30
        ASSERT(addr1.getBlockOffset(blockOffsetBits1) == 0x30);

        // Bits 6-13: index = 0x7B
        ASSERT(addr1.getIndex(blockOffsetBits1, indexBits1) == 0x7B);

        // Bits 14+: tag = 0x48D159E26AF3
        ASSERT(addr1.getTag(blockOffsetBits1, indexBits1) == 0x48D159E26AF3);

        // Configuration 2: 4-bit block offset, 10-bit index
        uint8_t blockOffsetBits2 = 4;
        uint8_t indexBits2 = 10;

        MemoryAddress addr2(0x1234);

        // Bits 0-3: block offset = 0x4
        ASSERT(addr2.getBlockOffset(blockOffsetBits2) == 0x4);

        // Bits 4-13: index = 0x23
        ASSERT(addr2.getIndex(blockOffsetBits2, indexBits2) == 0x123);

        // Bits 14+: tag = 0x4
        ASSERT(addr2.getTag(blockOffsetBits2, indexBits2) == 0x0);

        // Edge case: 0-bit configurations
        ASSERT(addr1.getBlockOffset(0) == 0);     // No offset bits
        ASSERT(addr1.getIndex(0, 0) == 0);        // No index bits
        ASSERT(addr1.getTag(0, 0) == 0x123456789ABCDEF0); // Entire address is tag

        // Edge case: Large configurations
        ASSERT(addr1.getBlockOffset(64) == 0x123456789ABCDEF0); // All bits are offset
        //ASSERT(addr1.getBlockOffset(64) == 0x123456789ABCDEF0); // All bits are offset
        ASSERT(addr1.getIndex(0, 64) == 0x123456789ABCDEF0);   // All bits are index
        ASSERT(addr1.getTag(64, 0) == 0);                       // No bits left for tag

        // Test with simple addresses for verification
        MemoryAddress simpleAddr(0xFF);  // Binary: 11111111
        ASSERT(simpleAddr.getBlockOffset(3) == 0x7);  // Last 3 bits: 111
        ASSERT(simpleAddr.getIndex(3, 2) == 0x3);     // Next 2 bits: 11
        ASSERT(simpleAddr.getTag(3, 2) == 0x7);       // Remaining bits: 111
    }
    END_TEST;
}

// Test 4: Test string representation
void testToString() {
    TEST_CASE("toString functionality") {
        // Test various addresses
        MemoryAddress addr1(0x0);
        ASSERT(addr1.toString() == "0x0000000000000000");

        MemoryAddress addr2(0x123456789ABCDEF0);
        ASSERT(addr2.toString() == "0x123456789abcdef0");

        MemoryAddress addr3(0xFFFFFFFFFFFFFFFF);
        ASSERT(addr3.toString() == "0xffffffffffffffff");

        MemoryAddress addr4(0x1);
        ASSERT(addr4.toString() == "0x0000000000000001");

        // Test with specifically formatted values
        MemoryAddress addr5(0xABCD);
        ASSERT(addr5.toString() == "0x000000000000abcd");
    }
    END_TEST;
}

// Test 5: Test equality operators
void testEqualityOperators() {
    TEST_CASE("Equality operators") {
        MemoryAddress addr1(0x1234);
        MemoryAddress addr2(0x1234);
        MemoryAddress addr3(0x5678);

        // Test equality
        ASSERT(addr1 == addr2);
        ASSERT(!(addr1 == addr3));

        // Test inequality
        ASSERT(!(addr1 != addr2));
        ASSERT(addr1 != addr3);

        // Test with edge cases
        MemoryAddress zero1(0);
        MemoryAddress zero2(0);
        ASSERT(zero1 == zero2);

        MemoryAddress max1(std::numeric_limits<uint64_t>::max());
        MemoryAddress max2(std::numeric_limits<uint64_t>::max());
        ASSERT(max1 == max2);
    }
    END_TEST;
}

// Test 6: Test hash function support
void testHashFunction() {
    TEST_CASE("Hash function support") {
        // Create addresses and store in unordered_map
        std::unordered_map<MemoryAddress, int> map;

        MemoryAddress addr1(0x1234);
        MemoryAddress addr2(0x5678);
        MemoryAddress addr3(0x1234); // Same as addr1

        map[addr1] = 100;
        map[addr2] = 200;

        // Test retrieval
        ASSERT(map[addr1] == 100);
        ASSERT(map[addr2] == 200);
        ASSERT(map[addr3] == 100); // Should find the same entry as addr1

        // Test hash collisions (though unlikely with simple hash)
        MemoryAddress addr4(0);
        MemoryAddress addr5(std::numeric_limits<uint64_t>::max());

        map[addr4] = 300;
        map[addr5] = 400;

        ASSERT(map[addr4] == 300);
        ASSERT(map[addr5] == 400);

        // Verify size
        ASSERT(map.size() == 4);
    }
    END_TEST;
}

// Test 7: Boundary and edge cases
void testBoundaryAndEdgeCases() {
    TEST_CASE("Boundary and edge cases") {
        // Maximum uint64_t value
        MemoryAddress maxAddr(std::numeric_limits<uint64_t>::max());
        ASSERT(maxAddr.getAddress() == std::numeric_limits<uint64_t>::max());
        ASSERT(maxAddr.getBits(0, 63) == std::numeric_limits<uint64_t>::max());

        // Minimum value (0)
        MemoryAddress minAddr(0);
        ASSERT(minAddr.getAddress() == 0);
        ASSERT(minAddr.getBits(0, 63) == 0);

        // Test with negative values (should convert properly)
        MemoryAddress negAddr(static_cast<int32_t>(-1));
        ASSERT(negAddr.getAddress() == 0xFFFFFFFFFFFFFFFF);

        // Test extraction at boundaries
        MemoryAddress boundaryAddr(0x8000000000000000); // MSB set
        ASSERT(boundaryAddr.getBits(63, 63) == 1);
        ASSERT(boundaryAddr.getBits(0, 62) == 0);

        // Test with alternating pattern
        MemoryAddress patternAddr(0xAAAAAAAAAAAAAAAA);
        ASSERT(patternAddr.getBits(0, 3) == 0xA);
        ASSERT(patternAddr.getBits(4, 7) == 0xA);
    }
    END_TEST;
}

// Main test runner
int main() {
    std::cout << "Running MemoryAddress test suite..." << std::endl;
    std::cout << "===================================" << std::endl;

    testConstructorWithDifferentTypes();
    testGetBits();
    testCacheFieldExtraction();
    testToString();
    testEqualityOperators();
    testHashFunction();
    testBoundaryAndEdgeCases();

    std::cout << "\nAll tests completed." << std::endl;
    return 0;
}
