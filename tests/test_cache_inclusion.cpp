// test_cache_inclusion.cpp - Corrected test cases

#include <iostream>
#include <vector>
#include "cache.hpp"
#include "memory.hpp"

// Test 1: Corrected Inclusive Cache Test
void testInclusiveBasicProperty() {
    std::cout << "\n=== Test 1: Basic Inclusive Cache Property ===\n";

    // Create cache configs
    CacheConfig l1Config;
    l1Config.level = 1;
    l1Config.size = 4096;
    l1Config.blockSize = 64;
    l1Config.associativity = 4;
    l1Config.accessLatency = 1;

    CacheConfig l2Config;
    l2Config.level = 2;
    l2Config.size = 8192;
    l2Config.blockSize = 64;
    l2Config.associativity = 8;
    l2Config.accessLatency = 10;
    l2Config.inclusionPolicy = InclusionPolicy::Inclusive;

    // Create cache hierarchy
    CacheHierarchy hierarchy;
    hierarchy.addCacheLevel(std::make_unique<Cache>(l1Config));
    hierarchy.addCacheLevel(std::make_unique<Cache>(l2Config));

    // Step 1: Initial access to a block
    std::cout << "Step 1: Initial access to block 0x1000\n";
    auto result = hierarchy.access(MemoryAddress(0x1000), AccessType::Read);
    std::cout << "  Result: " << (result.second ? "Hit" : "Miss") << ", Latency: " << result.first << " cycles\n";

    // Check if block is in both L1 and L2 (inclusive property)
    bool inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(0x1000));
    bool inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(0x1000));
    std::cout << "  Block 0x1000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // CORRECT CHECK: For inclusive caches, blocks in L1 must also be in L2
    if (inL1 && inL2) {
        std::cout << "✓ Inclusive property verified: Block in L1 is also in L2\n";
    } else if (inL1 && !inL2) {
        std::cout << "✗ Incorrect behavior: Block in L1 but not in L2 (violates inclusion)\n";
    }

    // Step 2: Force eviction from L1
    std::cout << "\nStep 2: Filling L1 cache to force eviction from L1...\n";
    uint64_t baseAddr = 0x1000;  // Target set 0
    uint64_t numSets = 16;       // 16 sets (4KB/64B/4-way)
    uint64_t blockSize = 64;     // 64B blocks

    // Access blocks that map to the same set in L1
    for (int i = 1; i <= 5; i++) {  // 5 blocks for a 4-way cache = 1 eviction
        uint64_t addr = baseAddr + (i * numSets * blockSize);
        auto fillResult = hierarchy.access(MemoryAddress(addr), AccessType::Read);
        std::cout << "  Accessing 0x" << std::hex << addr << std::dec
                  << ": " << (fillResult.second ? "Hit" : "Miss") << "\n";
    }

    // Check state of original block
    inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(baseAddr));
    inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(baseAddr));
    std::cout << "  Block 0x1000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // CORRECT CHECK: For inclusive caches, blocks evicted from L1 can remain in L2
    if (!inL1 && inL2) {
        std::cout << "✓ Correct inclusive behavior: Block evicted from L1 can remain in L2\n";
    } else if (!inL1 && !inL2) {
        std::cout << "? Note: Block was evicted from both L1 and L2. This can happen but is not required for inclusive caches.\n";
    }

    // Step 3: Force eviction from L2
    std::cout << "\nStep 3: Testing backinvalidation (evict from L2)...\n";

    // First, refill L1 and L2 sets with known blocks
    std::cout << "  Refilling L1 and L2 with new blocks...\n";
    for (int i = 0; i < 8; i++) {  // 8 blocks to fill L2 set
        uint64_t addr = 0x3000 + (i * numSets * blockSize);  // Different set than before
        auto fillResult = hierarchy.access(MemoryAddress(addr), AccessType::Read);
    }

    // Get one of the blocks we know should be in both L1 and L2
    uint64_t testAddr = 0x3000;
    inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(testAddr));
    inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(testAddr));
    std::cout << "  Block 0x3000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // Now add more blocks to force eviction from L2
    std::cout << "  Forcing eviction from L2...\n";
    for (int i = 8; i < 16; i++) {  // 8 more blocks to cause eviction in L2
        uint64_t addr = 0x3000 + (i * numSets * blockSize);
        auto fillResult = hierarchy.access(MemoryAddress(addr), AccessType::Read);
    }

    // Check if our test block was evicted from L2, and if it was also removed from L1
    inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(testAddr));
    inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(testAddr));
    std::cout << "  After L2 eviction, block 0x3000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // CORRECT CHECK: For inclusive caches, if a block is evicted from L2, it must also be removed from L1
    if (!inL2 && inL1) {
        std::cout << "✗ Incorrect behavior: Block in L1 but not in L2 (violates inclusion)\n";
    } else if (!inL2 && !inL1) {
        std::cout << "✓ Backinvalidation working: Block evicted from L2 is also removed from L1\n";
    }
}

// Test 2: Corrected Exclusive Cache Test
void testExclusiveBasicProperty() {
    std::cout << "\n=== Test 2: Basic Exclusive Cache Property ===\n";

    // Create cache configs
    CacheConfig l1Config;
    l1Config.level = 1;
    l1Config.size = 4096;
    l1Config.blockSize = 64;
    l1Config.associativity = 4;
    l1Config.accessLatency = 1;

    CacheConfig l2Config;
    l2Config.level = 2;
    l2Config.size = 8192;
    l2Config.blockSize = 64;
    l2Config.associativity = 8;
    l2Config.accessLatency = 10;
    l2Config.inclusionPolicy = InclusionPolicy::Exclusive;

    // Create cache hierarchy
    CacheHierarchy hierarchy;
    hierarchy.addCacheLevel(std::make_unique<Cache>(l1Config));
    hierarchy.addCacheLevel(std::make_unique<Cache>(l2Config));

    // Step 1: Initial memory access
    std::cout << "Step 1: Initial access to block 0x1000\n";
    auto result = hierarchy.access(MemoryAddress(0x1000), AccessType::Read);
    std::cout << "  Result: " << (result.second ? "Hit" : "Miss") << ", Latency: " << result.first << " cycles\n";

    // Check state after access
    bool inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(0x1000));
    bool inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(0x1000));
    std::cout << "  Block 0x1000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // CORRECT CHECK: For exclusive caches, after initial miss, the block should be in L1 only
    if (inL1 && !inL2) {
        std::cout << "✓ Exclusive property verified: Block in L1 only\n";
    } else if (inL1 && inL2) {
        std::cout << "✗ Incorrect behavior: Block in both L1 and L2 (should be exclusive)\n";
    }

    // Step 2: Force eviction from L1
    std::cout << "\nStep 2: Filling L1 cache to force eviction...\n";
    uint64_t baseAddr = 0x1000;  // Target set 0
    uint64_t numSets = 16;       // 16 sets (4KB/64B/4-way)
    uint64_t blockSize = 64;     // 64B blocks

    // Access blocks that map to the same set to force eviction
    for (int i = 1; i <= 5; i++) {  // 5 blocks for a 4-way cache = 1 eviction
        uint64_t addr = baseAddr + (i * numSets * blockSize);
        auto fillResult = hierarchy.access(MemoryAddress(addr), AccessType::Read);
        std::cout << "  Accessing 0x" << std::hex << addr << std::dec
                  << ": " << (fillResult.second ? "Hit" : "Miss") << "\n";
    }

    // Check state of original block
    inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(baseAddr));
    inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(baseAddr));
    std::cout << "  Block 0x1000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // CORRECT CHECK: For exclusive caches, blocks evicted from L1 should move to L2
    if (!inL1 && inL2) {
        std::cout << "✓ Victim caching working: Block evicted from L1 moved to L2\n";
    } else if (!inL1 && !inL2) {
        std::cout << "✗ Incorrect behavior: Block lost from both caches (should be in L2)\n";
    } else if (inL1) {
        std::cout << "✗ Unexpected behavior: Block still in L1 (should have been evicted)\n";
    }

    // Step 3: Re-access the evicted block
    std::cout << "\nStep 3: Re-accessing the evicted block\n";
    result = hierarchy.access(MemoryAddress(baseAddr), AccessType::Read);
    std::cout << "  Result: " << (result.second ? "Hit" : "Miss") << ", Latency: " << result.first << " cycles\n";

    // Check final state
    inL1 = hierarchy.getCacheLevel(0).contains(MemoryAddress(baseAddr));
    inL2 = hierarchy.getCacheLevel(1).contains(MemoryAddress(baseAddr));
    std::cout << "  Block 0x1000 is " << (inL1 ? "in" : "not in") << " L1, "
              << (inL2 ? "in" : "not in") << " L2\n";

    // CORRECT CHECK: For exclusive caches, on L2 hit, the block should move from L2 to L1
    if (inL1 && !inL2) {
        std::cout << "✓ Exclusive transfer working: Block moved from L2 to L1\n";
    } else if (!inL1 && inL2) {
        std::cout << "✗ Incorrect behavior: Block still in L2 (should have moved to L1)\n";
    } else if (inL1 && inL2) {
        std::cout << "✗ Incorrect behavior: Block in both levels (violates exclusion)\n";
    } else if (!inL1 && !inL2) {
        std::cout << "✗ Incorrect behavior: Block lost from both caches\n";
    }
}

// Test 3: Comparative Performance
void testComparativePerformance() {
    std::cout << "\n=== Test 3: Comparative Performance ===\n";

    // Create common config parameters
    uint64_t l1Size = 8192;      // 8KB L1
    uint64_t l2Size = 16384;     // 16KB L2
    uint64_t blockSize = 64;     // 64B blocks
    uint64_t totalMemory = 128 * 1024;  // 128KB address space

    // Create inclusive hierarchy
    CacheConfig l1IncConfig;
    l1IncConfig.level = 1;
    l1IncConfig.size = l1Size;
    l1IncConfig.blockSize = blockSize;
    l1IncConfig.associativity = 4;
    l1IncConfig.accessLatency = 1;

    CacheConfig l2IncConfig;
    l2IncConfig.level = 2;
    l2IncConfig.size = l2Size;
    l2IncConfig.blockSize = blockSize;
    l2IncConfig.associativity = 8;
    l2IncConfig.accessLatency = 10;
    l2IncConfig.inclusionPolicy = InclusionPolicy::Inclusive;

    CacheHierarchy inclusiveHierarchy;
    inclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l1IncConfig));
    inclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l2IncConfig));

    // Create exclusive hierarchy (same parameters)
    CacheConfig l1ExcConfig = l1IncConfig;
    CacheConfig l2ExcConfig = l2IncConfig;
    l2ExcConfig.inclusionPolicy = InclusionPolicy::Exclusive;

    CacheHierarchy exclusiveHierarchy;
    exclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l1ExcConfig));
    exclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l2ExcConfig));

    // Test 1: Working Set Larger than L1 but Smaller than L1+L2
    std::cout << "Test with working set > L1 but < L1+L2\n";

    // Working set = 32KB (larger than L1's 8KB, smaller than L1+L2's 24KB)
    int workingSetSize = 32 * 1024;
    int numAccesses = 1000000;

    // First, use inclusive hierarchy
    std::cout << "Running inclusive cache simulation...\n";
    uint64_t inclusiveTotalLatency = 0;
    int inclusiveHits = 0;

    for (int i = 0; i < numAccesses; i++) {
        // Use a strided access pattern to better show differences
        // This will access every 16 bytes, wrapping around the working set
        uint64_t addr = (i * 16) % workingSetSize;
        auto result = inclusiveHierarchy.access(MemoryAddress(addr), AccessType::Read);
        inclusiveTotalLatency += result.first;
        if (result.second) inclusiveHits++;
    }

    double inclusiveHitRate = static_cast<double>(inclusiveHits) / numAccesses * 100.0;
    double inclusiveAvgLatency = static_cast<double>(inclusiveTotalLatency) / numAccesses;

    // Reset and run with exclusive hierarchy
    exclusiveHierarchy.reset();

    std::cout << "Running exclusive cache simulation...\n";
    uint64_t exclusiveTotalLatency = 0;
    int exclusiveHits = 0;

    for (int i = 0; i < numAccesses; i++) {
        uint64_t addr = (i * 16) % workingSetSize;
        auto result = exclusiveHierarchy.access(MemoryAddress(addr), AccessType::Read);
        exclusiveTotalLatency += result.first;
        if (result.second) exclusiveHits++;
    }

    double exclusiveHitRate = static_cast<double>(exclusiveHits) / numAccesses * 100.0;
    double exclusiveAvgLatency = static_cast<double>(exclusiveTotalLatency) / numAccesses;

    // Compare results
    std::cout << "Inclusive: Hit Rate = " << inclusiveHitRate << "%, Avg Latency = "
              << inclusiveAvgLatency << " cycles\n";
    std::cout << "Exclusive: Hit Rate = " << exclusiveHitRate << "%, Avg Latency = "
              << exclusiveAvgLatency << " cycles\n";

    // Exclusive should have better hit rate and lower latency for this working set
    if (exclusiveHitRate > inclusiveHitRate + 1.0) {  // Add a small margin
        std::cout << "✓ Exclusive cache has better hit rate as expected\n";
    } else {
        std::cout << "✗ Exclusive cache does NOT have better hit rate (unexpected)\n";
    }

    // Reset both hierarchies
    inclusiveHierarchy.reset();
    exclusiveHierarchy.reset();

    // Test 2: Working Set Smaller than L1
    std::cout << "\nTest with working set < L1\n";

    // Working set = 4KB (fits in L1)
    workingSetSize = 4 * 1024;

    // Run inclusive hierarchy
    inclusiveTotalLatency = 0;
    inclusiveHits = 0;

    for (int i = 0; i < numAccesses; i++) {
        uint64_t addr = (i * 16) % workingSetSize;
        auto result = inclusiveHierarchy.access(MemoryAddress(addr), AccessType::Read);
        inclusiveTotalLatency += result.first;
        if (result.second) inclusiveHits++;
    }

    inclusiveHitRate = static_cast<double>(inclusiveHits) / numAccesses * 100.0;
    inclusiveAvgLatency = static_cast<double>(inclusiveTotalLatency) / numAccesses;

    // Run exclusive hierarchy
    exclusiveTotalLatency = 0;
    exclusiveHits = 0;

    for (int i = 0; i < numAccesses; i++) {
        uint64_t addr = (i * 16) % workingSetSize;
        auto result = exclusiveHierarchy.access(MemoryAddress(addr), AccessType::Read);
        exclusiveTotalLatency += result.first;
        if (result.second) exclusiveHits++;
    }

    exclusiveHitRate = static_cast<double>(exclusiveHits) / numAccesses * 100.0;
    exclusiveAvgLatency = static_cast<double>(exclusiveTotalLatency) / numAccesses;

    // Compare results
    std::cout << "Inclusive: Hit Rate = " << inclusiveHitRate << "%, Avg Latency = "
              << inclusiveAvgLatency << " cycles\n";
    std::cout << "Exclusive: Hit Rate = " << exclusiveHitRate << "%, Avg Latency = "
              << exclusiveAvgLatency << " cycles\n";

    // Both should have similar hit rates in this scenario
    if (std::abs(exclusiveHitRate - inclusiveHitRate) < 1.0) {
        std::cout << "✓ Both caches have similar hit rates for working set < L1 (correct)\n";
    } else {
        std::cout << "✗ Caches have significantly different hit rates (unexpected)\n";
    }
}

void testMixedWorkingSet() {
    std::cout << "\n=== Test: Mixed Working Set (Small + Large) ===\n";

    // Cache configurations
    CacheConfig l1IncConfig, l2IncConfig;
    l1IncConfig.level = 1;
    l1IncConfig.size = 8192;  // 8KB
    l1IncConfig.blockSize = 64;
    l1IncConfig.associativity = 4;
    l1IncConfig.accessLatency = 1;

    l2IncConfig.level = 2;
    l2IncConfig.size = 32768;  // 32KB
    l2IncConfig.blockSize = 64;
    l2IncConfig.associativity = 8;
    l2IncConfig.accessLatency = 10;
    l2IncConfig.inclusionPolicy = InclusionPolicy::Inclusive;

    // Create the inclusive hierarchy
    CacheHierarchy inclusiveHierarchy;
    inclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l1IncConfig));
    inclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l2IncConfig));

    // Create the exclusive hierarchy with the same parameters
    CacheConfig l1ExcConfig = l1IncConfig;
    CacheConfig l2ExcConfig = l2IncConfig;
    l2ExcConfig.inclusionPolicy = InclusionPolicy::Exclusive;

    CacheHierarchy exclusiveHierarchy;
    exclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l1ExcConfig));
    exclusiveHierarchy.addCacheLevel(std::make_unique<Cache>(l2ExcConfig));

    // Access pattern: 70% to small 4KB working set, 30% to larger 12KB set
    const int primarySetSize = 4 * 1024;   // 4KB
    const int secondarySetSize = 12 * 1024; // 12KB
    const int totalSize = primarySetSize + secondarySetSize;
    const int numAccesses = 1000000;
    const double primaryRatio = 0.7;  // 70% of accesses

    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // Run with inclusive hierarchy
    std::cout << "Running inclusive cache simulation...\n";
    uint64_t inclusiveTotalLatency = 0;
    int inclusiveHits = 0;

    for (int i = 0; i < numAccesses; i++) {
        uint64_t addr;
        if (dist(rng) < primaryRatio) {
            // Access primary working set (more frequently accessed)
            addr = (i % (primarySetSize / 4)) * 4;
        } else {
            // Access secondary working set
            addr = primarySetSize + (i % (secondarySetSize / 4)) * 4;
        }

        auto result = inclusiveHierarchy.access(MemoryAddress(addr), AccessType::Read);
        inclusiveTotalLatency += result.first;
        if (result.second) inclusiveHits++;
    }

    double inclusiveHitRate = static_cast<double>(inclusiveHits) / numAccesses * 100.0;
    double inclusiveAvgLatency = static_cast<double>(inclusiveTotalLatency) / numAccesses;

    // Reset RNG to get the same sequence
    rng.seed(42);

    // Run with exclusive hierarchy
    std::cout << "Running exclusive cache simulation...\n";
    uint64_t exclusiveTotalLatency = 0;
    int exclusiveHits = 0;

    for (int i = 0; i < numAccesses; i++) {
        uint64_t addr;
        if (dist(rng) < primaryRatio) {
            // Access primary working set
            addr = (i % (primarySetSize / 4)) * 4;
        } else {
            // Access secondary working set
            addr = primarySetSize + (i % (secondarySetSize / 4)) * 4;
        }

        auto result = exclusiveHierarchy.access(MemoryAddress(addr), AccessType::Read);
        exclusiveTotalLatency += result.first;
        if (result.second) exclusiveHits++;
    }

    double exclusiveHitRate = static_cast<double>(exclusiveHits) / numAccesses * 100.0;
    double exclusiveAvgLatency = static_cast<double>(exclusiveTotalLatency) / numAccesses;

    // Report results
    std::cout << "=== Results for Mixed Working Set Test ===\n";
    std::cout << "Inclusive Cache: Hit Rate = " << inclusiveHitRate << "%, Avg Latency = "
              << inclusiveAvgLatency << " cycles\n";
    std::cout << "Exclusive Cache: Hit Rate = " << exclusiveHitRate << "%, Avg Latency = "
              << exclusiveAvgLatency << " cycles\n";

    // Exclusive should have better hit rate for this mixed working set
    if (exclusiveHitRate > inclusiveHitRate + 1.0) {
        std::cout << "✓ Exclusive cache has better hit rate as expected (+"
                  << (exclusiveHitRate - inclusiveHitRate) << "%)\n";
        std::cout << "  This is because exclusive caching increases effective cache capacity.\n";
    } else {
        std::cout << "- Similar hit rates observed. Try increasing the working set size.\n";
    }
}

int main() {
    std::cout << "Running Inclusive/Exclusive Cache Tests\n";

    try {
        // Run the test cases
        testInclusiveBasicProperty();
        testExclusiveBasicProperty();
        testComparativePerformance();
        //testMixedWorkingSet();

        std::cout << "\nAll tests completed!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
