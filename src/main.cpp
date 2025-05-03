// main.cpp
#include "address.hpp"
#include "cache.hpp"
#include "memory.hpp"
#include "metrics.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <thread>
#include <functional>

// Helper function to create a cache configuration
CacheConfig createCacheConfig(
    CacheConfig::Organization org,
    uint64_t size,
    uint64_t blockSize,
    uint64_t associativity,
    const std::string& policy,
    uint64_t latency,
    bool writeBack,
    bool writeAllocate) {
    
    CacheConfig config;
    config.organization = org;
    config.size = size;
    config.blockSize = blockSize;
    config.associativity = associativity;
    config.policy = policy;
    config.accessLatency = latency;
    config.writeBack = writeBack;
    config.writeAllocate = writeAllocate;
    
    return config;
}

// Helper function to create a trace file for testing
void createTestTraceFile(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not create trace file: " << filename << std::endl;
        return;
    }
    
    // Generate a memory access pattern
    // Format: <address> <R/W>
    
    // Sequential reads
    for (uint64_t i = 0; i < 1000; i += 64) {
        file << std::hex << i << " R" << std::endl;
    }
    
    // Random reads and writes
    std::srand(42); // Fixed seed for reproducibility
    for (int i = 0; i < 1000; ++i) {
        uint64_t addr = std::rand() % 0x100000; // 1MB address space
        char type = (std::rand() % 100 < 70) ? 'R' : 'W'; // 70% reads, 30% writes
        file << std::hex << addr << " " << type << std::endl;
    }
    
    // Looping pattern
    for (int j = 0; j < 10; ++j) {
        for (uint64_t i = 0x200000; i < 0x201000; i += 64) {
            file << std::hex << i << " R" << std::endl;
        }
    }
    
    file.close();
    std::cout << "Created test trace file: " << filename << std::endl;
}

// Helper function to run a simple test to verify cache functionality
void runSingleTest() {
    // Create L1 cache (direct-mapped, 32KB, 64B blocks)
    auto l1Config = createCacheConfig(
        CacheConfig::Organization::DirectMapped,
        32 * 1024,      // 32KB
        64,             // 64B blocks
        1,              // Direct-mapped
        "LRU",          // Policy (not used for direct-mapped)
        1,              // 1 cycle latency
        true,           // Write-back
        true            // Write-allocate
    );
    
    // Create cache
    Cache l1Cache(l1Config);
    
    // Create a simple access pattern
    std::vector<MemoryAccess> accesses = {
        // Sequential reads
        MemoryAccess(MemoryAddress(0x1000), AccessType::Read),
        MemoryAccess(MemoryAddress(0x1040), AccessType::Read),
        MemoryAccess(MemoryAddress(0x1080), AccessType::Read),
        MemoryAccess(MemoryAddress(0x10C0), AccessType::Read),
        
        // Repeated access (should hit)
        MemoryAccess(MemoryAddress(0x1000), AccessType::Read),
        
        // Write to existing block (should hit)
        MemoryAccess(MemoryAddress(0x1040), AccessType::Write),
        
        // Access to new location (should miss)
        MemoryAccess(MemoryAddress(0x2000), AccessType::Read),
        
        // Access that will cause conflict (should evict 0x1000 and miss)
        // For a 32KB direct-mapped cache with 64B blocks, index bits are 9 (512 sets),
        // meaning addresses with the same bits 6-14 will map to the same set
        // 0x21000 = 0b10_0001_0000_0000_0000 and 0x1000 = 0b1_0000_0000_0000
        // They have the same index bits (6-14) so they map to the same set
        MemoryAccess(MemoryAddress(0x21000), AccessType::Read)
    };
    
    // Process accesses
    std::cout << "Running basic cache test..." << std::endl;
    for (const auto& access : accesses) {
        auto result = l1Cache.access(access.address, access.type);
        std::cout << "Access to " << access.address.toString() 
                 << " (" << (access.type == AccessType::Read ? "Read" : "Write") << "): "
                 << (result.hit ? "HIT" : "MISS");
        
        if (result.writeBack) {
            std::cout << " (Write-back";
            if (result.evictedAddress) {
                std::cout << " from " << result.evictedAddress->toString();
            }
            std::cout << ")";
        }
        
        std::cout << std::endl;
    }
    
    // Print statistics
    std::cout << "\nCache Statistics:" << std::endl;
    std::cout << "Hits: " << l1Cache.getHits() << std::endl;
    std::cout << "Misses: " << l1Cache.getMisses() << std::endl;
    std::cout << "Hit Rate: " << (l1Cache.getHitRate() * 100.0) << "%" << std::endl;
}

// Helper function to run a comparative analysis of different cache configurations
void runComparativeAnalysis() {
    // Create the performance analyzer
    PerformanceAnalyzer analyzer;
    
    // Create test trace file
    const std::string traceFile = "test_trace.txt";
    createTestTraceFile(traceFile);
    
    // Test 1: Direct-mapped L1 cache
    {
        auto l1Config = createCacheConfig(
            CacheConfig::Organization::DirectMapped,
            32 * 1024,      // 32KB
            64,             // 64B blocks
            1,              // Direct-mapped
            "LRU",          // Policy (not used for direct-mapped)
            1,              // 1 cycle latency
            true,           // Write-back
            true            // Write-allocate
        );
        
        std::vector<std::unique_ptr<Cache>> caches;
        caches.push_back(std::make_unique<Cache>(l1Config));
        
        auto memory = std::make_unique<MainMemory>(100);
        auto trace = std::make_unique<FileTraceSource>(traceFile);
        
        analyzer.addTest(std::make_unique<TestConfig>(
            "Direct-Mapped L1",
            std::move(caches),
            std::move(memory),
            std::move(trace)
        ));
    }
    
    // Test 2: 4-way set-associative L1 cache with LRU
    {
        auto l1Config = createCacheConfig(
            CacheConfig::Organization::SetAssociative,
            32 * 1024,      // 32KB
            64,             // 64B blocks
            4,              // 4-way set-associative
            "LRU",          // LRU policy
            1,              // 1 cycle latency
            true,           // Write-back
            true            // Write-allocate
        );
        
        std::vector<std::unique_ptr<Cache>> caches;
        caches.push_back(std::make_unique<Cache>(l1Config));
        
        auto memory = std::make_unique<MainMemory>(100);
        auto trace = std::make_unique<FileTraceSource>(traceFile);
        
        analyzer.addTest(std::make_unique<TestConfig>(
            "4-Way SA L1 (LRU)",
            std::move(caches),
            std::move(memory),
            std::move(trace)
        ));
    }
    
    // Test 3: 4-way set-associative L1 cache with FIFO
    {
        auto l1Config = createCacheConfig(
            CacheConfig::Organization::SetAssociative,
            32 * 1024,      // 32KB
            64,             // 64B blocks
            4,              // 4-way set-associative
            "FIFO",         // FIFO policy
            1,              // 1 cycle latency
            true,           // Write-back
            true            // Write-allocate
        );
        
        std::vector<std::unique_ptr<Cache>> caches;
        caches.push_back(std::make_unique<Cache>(l1Config));
        
        auto memory = std::make_unique<MainMemory>(100);
        auto trace = std::make_unique<FileTraceSource>(traceFile);
        
        analyzer.addTest(std::make_unique<TestConfig>(
            "4-Way SA L1 (FIFO)",
            std::move(caches),
            std::move(memory),
            std::move(trace)
        ));
    }
    
    // Test 4: 4-way set-associative L1 cache with Random
    {
        auto l1Config = createCacheConfig(
            CacheConfig::Organization::SetAssociative,
            32 * 1024,      // 32KB
            64,             // 64B blocks
            4,              // 4-way set-associative
            "RANDOM",       // Random policy
            1,              // 1 cycle latency
            true,           // Write-back
            true            // Write-allocate
        );
        
        std::vector<std::unique_ptr<Cache>> caches;
        caches.push_back(std::make_unique<Cache>(l1Config));
        
        auto memory = std::make_unique<MainMemory>(100);
        auto trace = std::make_unique<FileTraceSource>(traceFile);
        
        analyzer.addTest(std::make_unique<TestConfig>(
            "4-Way SA L1 (Random)",
            std::move(caches),
            std::move(memory),
            std::move(trace)
        ));
    }
    
    // Test 5: L1 + L2 hierarchy
    {
        auto l1Config = createCacheConfig(
            CacheConfig::Organization::SetAssociative,
            32 * 1024,      // 32KB
            64,             // 64B blocks
            4,              // 4-way set-associative
            "LRU",          // LRU policy
            1,              // 1 cycle latency
            true,           // Write-back
            true            // Write-allocate
        );
        
        auto l2Config = createCacheConfig(
            CacheConfig::Organization::SetAssociative,
            256 * 1024,     // 256KB
            64,             // 64B blocks
            8,              // 8-way set-associative
            "LRU",          // LRU policy
            10,             // 10 cycle latency
            true,           // Write-back
            true            // Write-allocate
        );
        
        std::vector<std::unique_ptr<Cache>> caches;
        caches.push_back(std::make_unique<Cache>(l1Config));
        caches.push_back(std::make_unique<Cache>(l2Config));
        
        auto memory = std::make_unique<MainMemory>(100);
        auto trace = std::make_unique<FileTraceSource>(traceFile);
        
        analyzer.addTest(std::make_unique<TestConfig>(
            "L1+L2 Hierarchy",
            std::move(caches),
            std::move(memory),
            std::move(trace)
        ));
    }
    
    // Run the tests
    auto results = analyzer.runTests();
    
    // Compare the results
    std::cout << "\nResults Comparison:" << std::endl;
    analyzer.compareResults(results);
    
    // Save results for analysis
    analyzer.saveResultsToCSV(results, "cache_results.csv");
    analyzer.exportForPlotting(results, "plots");
}

int main() {
    std::cout << "Cache Simulator - Processor Architecture Project" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    // Run a simple test to verify basic cache functionality
    runSingleTest();
    
    std::cout << "\nRunning comparative analysis of different cache configurations..." << std::endl;
    runComparativeAnalysis();
    
    return 0;
}
