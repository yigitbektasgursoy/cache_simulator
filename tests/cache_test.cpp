// cache_test.cpp
#include "cache.hpp"
#include "address.hpp"
#include "cache_policy.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <cmath>
#include <map>
#include <random>

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

// Test 1: CacheConfig calculations
void testCacheConfig() {
    TEST_CASE("CacheConfig calculations") {
        // Test direct-mapped configuration
        CacheConfig dmConfig;
        dmConfig.organization = CacheConfig::Organization::DirectMapped;
        dmConfig.size = 32768;      // 32KB
        dmConfig.blockSize = 64;    // 64B blocks
        dmConfig.associativity = 1; // Direct-mapped

        ASSERT(dmConfig.getNumSets() == 512);    // 32KB / 64B = 512 sets
        ASSERT(dmConfig.getNumWays() == 1);       // Direct-mapped has 1 way
        ASSERT(dmConfig.getBlockOffsetBits() == 6); // log2(64) = 6 bits
        ASSERT(dmConfig.getIndexBits() == 9);      // log2(512) = 9 bits

        // Test set-associative configuration
        CacheConfig saConfig;
        saConfig.organization = CacheConfig::Organization::SetAssociative;
        saConfig.size = 32768;      // 32KB
        saConfig.blockSize = 64;    // 64B blocks
        saConfig.associativity = 4; // 4-way set-associative

        ASSERT(saConfig.getNumSets() == 128);    // 32KB / (64B * 4) = 128 sets
        ASSERT(saConfig.getNumWays() == 4);       // 4-way
        ASSERT(saConfig.getBlockOffsetBits() == 6); // log2(64) = 6 bits
        ASSERT(saConfig.getIndexBits() == 7);      // log2(128) = 7 bits

        // Test fully-associative configuration
        CacheConfig faConfig;
        faConfig.organization = CacheConfig::Organization::FullyAssociative;
        faConfig.size = 1024;       // 1KB
        faConfig.blockSize = 64;    // 64B blocks

        ASSERT(faConfig.getNumSets() == 1);      // Fully associative has 1 set
        ASSERT(faConfig.getNumWays() == 16);     // 1KB / 64B = 16 ways
        ASSERT(faConfig.getBlockOffsetBits() == 6); // log2(64) = 6 bits
        ASSERT(faConfig.getIndexBits() == 0);      // No index bits for fully associative
    }
    END_TEST;
}

// Test 2: Basic Cache operations (read hits and misses)
void testBasicCacheOperations() {
    TEST_CASE("Basic Cache operations") {
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;          // 256 bytes
        config.blockSize = 64;      // 64 byte blocks
        config.associativity = 1;   // Direct-mapped
        config.policy = "LRU";      // Policy (not used for direct-mapped)
        config.accessLatency = 1;
        config.writeBack = true;
        config.writeAllocate = true;

        Cache cache(config);

        // Test initial miss
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        ASSERT(result.latency > config.accessLatency);  // Miss penalty is higher than hit
        ASSERT(!result.writeBack);
        ASSERT(!result.evictedAddress.has_value());

        // Test hit on same address
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        ASSERT(result.latency == config.accessLatency);  // Hit has base latency

        // Test hit on address in same block (0x20 is in same 64-byte block as 0x0)
        result = cache.access(MemoryAddress(0x20), AccessType::Read);
        ASSERT(result.hit);

        // Test miss on different block (0x100 is in a different set)
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);

        // Check statistics
        ASSERT(cache.getHits() == 2);
        ASSERT(cache.getMisses() == 2);
        ASSERT(cache.getAccesses() == 4);
        ASSERT(std::abs(cache.getHitRate() - 0.5) < 0.001);
    }
    END_TEST;
}

// Test 3: Write policies (write-back vs write-through)
void testWritePolicies() {
    TEST_CASE("Write policies") {
        // Test write-back policy
        CacheConfig wbConfig;
        wbConfig.organization = CacheConfig::Organization::DirectMapped;
        wbConfig.size = 256;
        wbConfig.blockSize = 64;
        wbConfig.writeBack = true;
        wbConfig.writeAllocate = true;

        Cache wbCache(wbConfig);

        // Write miss with write-allocate
        auto result = wbCache.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(!result.hit);
        ASSERT(!result.writeBack);

        // Write hit - should mark as dirty but not write back
        result = wbCache.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(result.hit);
        ASSERT(!result.writeBack);

        // Access conflicting address in same set - should trigger write-back
        result = wbCache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        ASSERT(result.writeBack);
        ASSERT(result.evictedAddress.has_value());

        // Test no-write-allocate
        CacheConfig nwaConfig = wbConfig;
        nwaConfig.writeAllocate = false;

        Cache nwaCache(nwaConfig);

        // Write miss without allocation
        result = nwaCache.access(MemoryAddress(0x200), AccessType::Write);
        ASSERT(!result.hit);

        // Subsequent read should still miss
        result = nwaCache.access(MemoryAddress(0x200), AccessType::Read);
        ASSERT(!result.hit);
    }
    END_TEST;
}


// Test 4: Set-associative cache behavior - CORRECTED VERSION
void testSetAssociativeCache() {
    TEST_CASE("Set-associative cache") {
        CacheConfig config;
        config.organization = CacheConfig::Organization::SetAssociative;
        config.size = 256;          // 256 bytes
        config.blockSize = 64;      // 64 byte blocks
        config.associativity = 2;   // 2-way set-associative
        config.policy = "LRU";

        Cache cache(config);

        // With 256 bytes, 64-byte blocks, and 2-way associativity:
        // Number of sets = 256 / (64 * 2) = 2 sets

        // These addresses map to the same set (set 1):
        MemoryAddress addr1(0x40);  // Index bit = 1
        MemoryAddress addr2(0xC0);  // Index bit = 1
        MemoryAddress addr3(0x140); // Index bit = 1

        // Access two different addresses mapping to same set
        auto result1 = cache.access(addr1, AccessType::Read);
        ASSERT(!result1.hit);  // First access is a miss

        auto result2 = cache.access(addr2, AccessType::Read);
        ASSERT(!result2.hit);  // Second access is also a miss

        // Both should now hit since the set has 2 ways
        result1 = cache.access(addr1, AccessType::Read);
        ASSERT(result1.hit);

        result2 = cache.access(addr2, AccessType::Read);
        ASSERT(result2.hit);

        // Access third address mapping to same set - should evict LRU (addr1)
        auto result3 = cache.access(addr3, AccessType::Read);
        ASSERT(!result3.hit);  // This is a miss

        // At this point, cache contains [addr2, addr3]
        // Check addr1 - should miss and evict addr2
        result1 = cache.access(addr1, AccessType::Read);
        ASSERT(!result1.hit);  // Was evicted, misses

        // At this point, cache contains [addr3, addr1]
        // Check addr2 - should miss and evict addr3
        result2 = cache.access(addr2, AccessType::Read);
        ASSERT(!result2.hit);  // Was evicted earlier, misses

        // At this point, cache contains [addr1, addr2]
        // Verify current state
        result1 = cache.access(addr1, AccessType::Read);
        ASSERT(result1.hit);   // Should hit now

        result2 = cache.access(addr2, AccessType::Read);
        ASSERT(result2.hit);   // Should hit now

        result3 = cache.access(addr3, AccessType::Read);
        ASSERT(!result3.hit);  // Should miss (was evicted)
    }
    END_TEST;
}

// Test 5: Cache hierarchy operations
void testCacheHierarchy() {
    TEST_CASE("Cache hierarchy") {
        // Create L1 cache (small, fast)
        CacheConfig l1Config;
        l1Config.organization = CacheConfig::Organization::SetAssociative;
        l1Config.size = 1024;       // 1KB
        l1Config.blockSize = 64;
        l1Config.associativity = 2;
        l1Config.policy = "LRU";
        l1Config.accessLatency = 1;
        l1Config.writeBack = true;
        l1Config.writeAllocate = true;

        // Create L2 cache (larger, slower)
        CacheConfig l2Config;
        l2Config.organization = CacheConfig::Organization::SetAssociative;
        l2Config.size = 4096;       // 4KB
        l2Config.blockSize = 64;
        l2Config.associativity = 4;
        l2Config.policy = "LRU";
        l2Config.accessLatency = 10;
        l2Config.writeBack = true;
        l2Config.writeAllocate = true;

        CacheHierarchy hierarchy;
        hierarchy.addCacheLevel(std::make_unique<Cache>(l1Config));
        hierarchy.addCacheLevel(std::make_unique<Cache>(l2Config));

        // Test L1 miss, L2 miss
        uint64_t latency = hierarchy.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(latency >= l1Config.accessLatency + l2Config.accessLatency);

        // Test L1 hit
        latency = hierarchy.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(latency == l1Config.accessLatency);

        // Get statistics
        auto stats = hierarchy.getStats();
        ASSERT(stats.size() == 2); // L1 and L2 stats
    }
    END_TEST;
}

// Test 6: Address mapping and conflict detection
void testAddressMapping() {
    TEST_CASE("Address mapping and conflicts") {
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 512;          // 512 bytes = 8 sets * 64 bytes
        config.blockSize = 64;      // 64 byte blocks

        Cache cache(config);

        // With 8 sets, we need 3 index bits
        // With 64-byte blocks, we need 6 offset bits

        // These addresses should map to the same set (set 0)
        MemoryAddress addr1(0x000);   // ...000 000000
        MemoryAddress addr2(0x200);   // ...010 000000 (different tag, same index)

        // This should map to a different set (set 1)
        MemoryAddress addr3(0x040);   // ...000 010000

        // Test conflicts
        cache.access(addr1, AccessType::Read);  // Miss, load addr1
        ASSERT(cache.access(addr1, AccessType::Read).hit);  // Hit on addr1

        // Access conflicting address (same set, different tag)
        cache.access(addr2, AccessType::Read);  // Miss, evict addr1, load addr2
        ASSERT(!cache.access(addr1, AccessType::Read).hit);  // addr1 was evicted
        ASSERT(!cache.access(addr2, AccessType::Read).hit);  // addr2 was just evicted by addr1!

        // Access addr2 again to have it in cache
        cache.access(addr2, AccessType::Read);
        ASSERT(cache.access(addr2, AccessType::Read).hit);   // Now addr2 is present

        // Different set - no conflict
        cache.access(addr3, AccessType::Read);  // Miss, load addr3
        ASSERT(cache.access(addr2, AccessType::Read).hit);   // addr2 still present (different set)
        ASSERT(cache.access(addr3, AccessType::Read).hit);   // addr3 also present
    }
    END_TEST;
}

// Test 7: Cache reset functionality
void testCacheReset() {
    TEST_CASE("Cache reset") {
        CacheConfig config;
        config.organization = CacheConfig::Organization::SetAssociative;
        config.size = 256;
        config.blockSize = 64;
        config.associativity = 2;

        Cache cache(config);

        // Perform some accesses
        cache.access(MemoryAddress(0x0), AccessType::Read);
        cache.access(MemoryAddress(0x40), AccessType::Read);
        cache.access(MemoryAddress(0x80), AccessType::Write);

        ASSERT(cache.getAccesses() > 0);
        ASSERT(cache.getHits() > 0 || cache.getMisses() > 0);

        // Reset the cache
        cache.reset();

        // Check that statistics are cleared
        ASSERT(cache.getHits() == 0);
        ASSERT(cache.getMisses() == 0);
        ASSERT(cache.getAccesses() == 0);

        // Check that entries are cleared
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);  // Should miss after reset
    }
    END_TEST;
}

// Test 8: Cache copy constructor
void testCacheCopyConstructor() {
    TEST_CASE("Cache copy constructor") {
        CacheConfig config;
        config.organization = CacheConfig::Organization::SetAssociative;
        config.size = 256;
        config.blockSize = 64;
        config.associativity = 2;
        config.policy = "LRU";

        Cache original(config);

        // Perform some accesses
        original.access(MemoryAddress(0x0), AccessType::Read);
        original.access(MemoryAddress(0x40), AccessType::Write);

        // Create a copy
        Cache copy(original);

        // Check that configuration is copied
        ASSERT(copy.getConfig().size == original.getConfig().size);
        ASSERT(copy.getConfig().blockSize == original.getConfig().blockSize);

        // Check that state is copied (both should have same hits/misses)
        ASSERT(copy.getHits() == original.getHits());
        ASSERT(copy.getMisses() == original.getMisses());

        // Check that copies are independent
        original.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(copy.getAccesses() != original.getAccesses());
    }
    END_TEST;
}

// Test 9: Edge cases and error conditions
void testEdgeCases() {
    TEST_CASE("Edge cases") {
        // Test with minimum size cache
        CacheConfig minConfig;
        minConfig.organization = CacheConfig::Organization::DirectMapped;
        minConfig.size = 64;        // Single block
        minConfig.blockSize = 64;

        Cache minCache(minConfig);

        // Both addresses map to the same block (single set with single way)
        minCache.access(MemoryAddress(0x0), AccessType::Read);   // Miss, load 0x0
        minCache.access(MemoryAddress(0x40), AccessType::Read);  // Miss, evict 0x0, load 0x40

        // Since cache only holds one block at a time, both will miss on second access
        auto result1 = minCache.access(MemoryAddress(0x0), AccessType::Read);   // Miss, evict 0x40, load 0x0
        auto result2 = minCache.access(MemoryAddress(0x40), AccessType::Read);  // Miss, evict 0x0, load 0x40

        ASSERT(!result1.hit);  // 0x0 was evicted by 0x40
        ASSERT(!result2.hit);  // 0x40 was evicted by 0x0

        // Check final statistics
        ASSERT(minCache.getHits() == 0);      // No hits possible with constant eviction
        ASSERT(minCache.getMisses() == 4);    // All 4 accesses were misses

        // Test with maximum address values
        MemoryAddress maxAddr(std::numeric_limits<uint64_t>::max());
        auto result = minCache.access(maxAddr, AccessType::Read);
        ASSERT(result.latency > 0);

        // Test with zero configurations (default constructor)
        CacheConfig defaultConfig;
        Cache defaultCache(defaultConfig);
        result = defaultCache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.latency > 0);
    }
    END_TEST;
}

// Test 10: Stress test with random access patterns
void testStressTest() {
    TEST_CASE("Stress test") {
        CacheConfig config;
        config.organization = CacheConfig::Organization::SetAssociative;
        config.size = 8192;         // 8KB
        config.blockSize = 64;
        config.associativity = 4;
        config.policy = "LRU";

        Cache cache(config);

        std::mt19937 rng(42);  // Fixed seed for reproducibility
        std::uniform_int_distribution<uint64_t> addrDist(0, 1 << 20);  // 1MB address space
        std::uniform_int_distribution<int> typeDist(0, 1);  // Read or Write

        const int numAccesses = 10000;

        for (int i = 0; i < numAccesses; i++) {
            MemoryAddress addr(addrDist(rng));
            AccessType type = typeDist(rng) ? AccessType::Write : AccessType::Read;

            auto result = cache.access(addr, type);

            // Basic sanity checks
            ASSERT(result.latency > 0);
            if (result.writeBack) {
                ASSERT(result.evictedAddress.has_value());
            }
        }

        // Verify statistics are reasonable
        ASSERT(cache.getAccesses() == numAccesses);
        ASSERT(cache.getHits() + cache.getMisses() == numAccesses);
        ASSERT(cache.getHitRate() >= 0.0 && cache.getHitRate() <= 1.0);
    }
    END_TEST;
}

// Main test runner
int main() {
    std::cout << "Running Cache test suite..." << std::endl;
    std::cout << "===========================" << std::endl;

    // Basic functionality tests
    testCacheConfig();
    testBasicCacheOperations();
    testWritePolicies();
    testSetAssociativeCache();

    // Advanced tests
    testCacheHierarchy();
    testAddressMapping();
    testCacheReset();
    testCacheCopyConstructor();

    // Edge cases and stress tests
    testEdgeCases();
    testStressTest();

    std::cout << "\nAll tests completed." << std::endl;
    return 0;
}
