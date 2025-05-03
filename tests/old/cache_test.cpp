// cache_test.cpp
#include "address.hpp"
#include "cache.hpp"
#include "cache_policy.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <string>

// Define a simple test framework
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

// Test memory address functionality
void testMemoryAddress() {
    TEST_CASE("MemoryAddress Construction and Bit Extraction") {
        MemoryAddress addr(0x12345678ABCDEF);
        
        // Test address storage
        ASSERT(addr.getAddress() == 0x12345678ABCDEF);
        
        // Test bit extraction
        ASSERT(addr.getBits(0, 7) == 0xEF);
        ASSERT(addr.getBits(8, 15) == 0xCD);
        ASSERT(addr.getBits(16, 23) == 0xAB);
        
        // Test tag/index/offset extraction for a specific cache configuration
        // For a cache with 64B blocks (6 bits offset) and 64 sets (6 bits index)
        uint8_t blockOffsetBits = 6;
        uint8_t indexBits = 6;
        
        // Expected values:
        // Block offset: bits 0-5 (0x2F = 0b101111)
        // Index: bits 6-11 (0x37 = 0b010011)
        // Tag: bits 12+ (0x12345678AB = 0b1001000110100010101100111000101010101011)
        MemoryAddress addr2(0x12345678ABCDEF);
        ASSERT(addr2.getBlockOffset(blockOffsetBits) == 0x2F);
        ASSERT(addr2.getIndex(blockOffsetBits, indexBits) == 0x37);
        ASSERT(addr2.getTag(blockOffsetBits, indexBits) == 0x12345678ABC);
    }
    END_TEST;
}

// Test cache configuration
void testCacheConfig() {
    TEST_CASE("CacheConfig Calculations") {
        // Direct-mapped cache
        CacheConfig dmConfig;
        dmConfig.organization = CacheConfig::Organization::DirectMapped;
        dmConfig.size = 32 * 1024; // 32KB
        dmConfig.blockSize = 64;   // 64B blocks
        dmConfig.associativity = 1; // Direct-mapped
        
        // Should have 32KB/64B = 512 sets
        ASSERT(dmConfig.getNumSets() == 512);
        ASSERT(dmConfig.getNumWays() == 1);
        ASSERT(dmConfig.getBlockOffsetBits() == 6); // 2^6 = 64
        ASSERT(dmConfig.getIndexBits() == 9);      // 2^9 = 512
        
        // Set-associative cache
        CacheConfig saConfig;
        saConfig.organization = CacheConfig::Organization::SetAssociative;
        saConfig.size = 32 * 1024; // 32KB
        saConfig.blockSize = 64;   // 64B blocks
        saConfig.associativity = 4; // 4-way set-associative
        
        // Should have 32KB/(64B*4) = 128 sets
        ASSERT(saConfig.getNumSets() == 128);
        ASSERT(saConfig.getNumWays() == 4);
        ASSERT(saConfig.getBlockOffsetBits() == 6); // 2^6 = 64
        ASSERT(saConfig.getIndexBits() == 7);      // 2^7 = 128
        
        // Fully associative cache
        CacheConfig faConfig;
        faConfig.organization = CacheConfig::Organization::FullyAssociative;
        faConfig.size = 32 * 1024; // 32KB
        faConfig.blockSize = 64;   // 64B blocks
        
        // Should have 1 set and 32KB/64B = 512 ways
        ASSERT(faConfig.getNumSets() == 1);
        ASSERT(faConfig.getNumWays() == 512);
        ASSERT(faConfig.getBlockOffsetBits() == 6); // 2^6 = 64
        ASSERT(faConfig.getIndexBits() == 0);      // No index bits for fully associative
    }
    END_TEST;
}

// Test LRU replacement policy
void testLRUPolicy() {
    TEST_CASE("LRU Replacement Policy") {
        LRUPolicy policy;
        
        // Access pattern: 0, 1, 2, 0, 3
        // LRU order after these accesses should be: 1, 2, 0, 3 (oldest to newest)
        policy.onAccess(0, 0); // Set 0, Way 0
        policy.onAccess(0, 1); // Set 0, Way 1
        policy.onAccess(0, 2); // Set 0, Way 2
        policy.onAccess(0, 0); // Set 0, Way 0 again
        policy.onAccess(0, 3); // Set 0, Way 3
        
        // With 4 ways, we should replace way 1 (least recently used)
        ASSERT(policy.getVictim(0, 4) == 1);
        
        // Access way 1, so way 2 becomes LRU
        policy.onAccess(0, 1);
        ASSERT(policy.getVictim(0, 4) == 2);
        
        // Test a different set
        policy.onAccess(1, 0); // Set 1, Way 0
        policy.onAccess(1, 1); // Set 1, Way 1
        
        // With 4 ways, but only 2 used, it should return an unused way
        ASSERT(policy.getVictim(1, 4) == 2 || policy.getVictim(1, 4) == 3);
        
        // Fill all ways in set 1
        policy.onAccess(1, 2); // Set 1, Way 2
        policy.onAccess(1, 3); // Set 1, Way 3
        
        // LRU is way 0
        ASSERT(policy.getVictim(1, 4) == 0);
    }
    END_TEST;
}

// Test FIFO replacement policy
void testFIFOPolicy() {
    TEST_CASE("FIFO Replacement Policy") {
        FIFOPolicy policy;
        
        // Access pattern: 0, 1, 2, 0, 3
        // FIFO order should still be: 0, 1, 2, 3 (oldest to newest)
        // Re-accessing 0 doesn't change its position
        policy.onAccess(0, 0); // Set 0, Way 0
        policy.onAccess(0, 1); // Set 0, Way 1
        policy.onAccess(0, 2); // Set 0, Way 2
        policy.onAccess(0, 0); // Set 0, Way 0 again - doesn't affect FIFO order
        policy.onAccess(0, 3); // Set 0, Way 3
        
        // With 4 ways, we should replace way 0 (first in)
        ASSERT(policy.getVictim(0, 4) == 0);
        
        // Even after accessing way 0 again, it should still be the victim
        policy.onAccess(0, 0);
        ASSERT(policy.getVictim(0, 4) == 0);
        
        // If we evict way 0 and insert a new way (4), then way 1 becomes the oldest
        policy.onAccess(0, 4);
        ASSERT(policy.getVictim(0, 5) == 0);
    }
    END_TEST;
}

// Test Random replacement policy
void testRandomPolicy() {
    TEST_CASE("Random Replacement Policy") {
        RandomPolicy policy;
        
        // Just check that it returns a valid way
        for (int i = 0; i < 100; ++i) {
            uint64_t victim = policy.getVictim(0, 4);
            ASSERT(victim < 4);
        }
    }
    END_TEST;
}

// Test direct-mapped cache
void testDirectMappedCache() {
    TEST_CASE("Direct-Mapped Cache") {
        // Create a tiny direct-mapped cache for testing
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.associativity = 1; // Direct-mapped
        config.policy = "LRU";    // Irrelevant for direct-mapped
        
        Cache cache(config);
        
        // This cache has 4 sets (256/64)
        // Addresses will map as follows:
        // Block offset: bits 0-5
        // Index: bits 6-7 (2 bits for 4 sets)
        // Tag: bits 8+
        
        // First access - miss
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // Same address - hit
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        
        // Different address, same set - miss and should evict the first block
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        
        // Now the first address should miss
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // Different address, different set - miss but shouldn't evict the previous block
        result = cache.access(MemoryAddress(0x40), AccessType::Read);
        ASSERT(!result.hit);
        
        // Previous address in the second set should still be cached
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(result.hit);
        
        // Check stats
        ASSERT(cache.getHits() == 2);
        ASSERT(cache.getMisses() == 4);
        ASSERT(cache.getHitRate() == 2.0 / 6.0);
    }
    END_TEST;
}

// Test set-associative cache
void testSetAssociativeCache() {
    TEST_CASE("Set-Associative Cache with LRU") {
        // Create a tiny 2-way set-associative cache
        CacheConfig config;
        config.organization = CacheConfig::Organization::SetAssociative;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.associativity = 2; // 2-way set-associative
        config.policy = "LRU";
        
        Cache cache(config);
        
        // This cache has 2 sets (256/(64*2))
        // Addresses will map as follows:
        // Block offset: bits 0-5
        // Index: bit 6 (1 bit for 2 sets)
        // Tag: bits 7+
        
        // First access to set 0 - miss
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // Second access to set 0 - miss, but doesn't evict the first block due to associativity
        result = cache.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(!result.hit);
        
        // Both addresses should now hit
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        
        result = cache.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(result.hit);
        
        // Third access to set 0 - miss and should evict the least recently used block (0x0)
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        
        // 0x0 should now miss
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // But 0x80 should still hit
        result = cache.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(result.hit);
        
        // Check stats
        ASSERT(cache.getHits() == 3);
        ASSERT(cache.getMisses() == 4);
    }
    END_TEST;
}

// Test write policies
void testWritePolicies() {
    TEST_CASE("Write-Back Policy") {
        // Create a write-back cache
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.writeBack = true;
        config.writeAllocate = true;
        
        Cache cache(config);
        
        // Read miss - block is loaded
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        ASSERT(!result.writeBack);
        
        // Write hit - block is marked dirty but no write-back
        result = cache.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(result.hit);
        ASSERT(!result.writeBack);
        
        // Evict the dirty block - should trigger write-back
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        ASSERT(result.writeBack);
        ASSERT(result.evictedAddress.has_value());
        ASSERT(result.evictedAddress->getAddress() == 0x0);
    }
    END_TEST;
    
    TEST_CASE("Write-Allocate Policy") {
        // Create a write-allocate cache
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.writeBack = true;
        config.writeAllocate = true;
        
        Cache cache(config);
        
        // Write miss - block is allocated
        auto result = cache.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(!result.hit);
        
        // Read hit - the block was allocated on the write miss
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        
        // Create a no-write-allocate cache
        config.writeAllocate = false;
        Cache cache2(config);
        
        // Write miss - block is not allocated (write goes directly to memory)
        result = cache2.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(!result.hit);
        
        // Read miss - the block was not allocated on the write miss
        result = cache2.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
    }
    END_TEST;
}

// Run all tests
int main() {
    std::cout << "Running cache simulator tests..." << std::endl;
    
    testMemoryAddress();
    testCacheConfig();
    testLRUPolicy();
    testFIFOPolicy();
    testRandomPolicy();
    testDirectMappedCache();
    testSetAssociativeCache();
    testWritePolicies();
    
    std::cout << "All tests completed." << std::endl;
    return 0;
}
