// tests/test_json_config.cpp
#include "cache.hpp"
#include "json_config.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json>" << std::endl;
        return 1;
    }

    try {
        // Load test configuration from JSON
        auto testConfig = JsonConfigLoader::loadTestConfig(argv[1]);

        std::cout << "Loaded test configuration: " << testConfig->name << std::endl;

        // Setup cache hierarchy
        CacheHierarchy hierarchy;
        for (const auto& cache : testConfig->caches) {
            hierarchy.addCacheLevel(std::make_unique<Cache>(*cache));
        }

        // Run the test
        auto& memory = testConfig->memory;
        auto& traceSource = testConfig->traceSource;

        uint64_t totalLatency = 0;
        uint64_t accessCount = 0;

        // Process all memory accesses
        while (auto access = traceSource->getNextAccess()) {
            // Access the cache hierarchy
            auto [latency, hitInCache] = hierarchy.access(access->address, access->type);

            // If cache miss at all levels, access main memory
            if (!hitInCache) {
                latency += memory->access(access->address, access->type);
            }

            totalLatency += latency;
            accessCount++;
        }

        // Print results
        std::cout << "Test complete. Processed " << accessCount << " memory accesses." << std::endl;
        if (accessCount > 0) {
            std::cout << "Average memory access time: "
                     << static_cast<double>(totalLatency) / accessCount
                     << " cycles" << std::endl;
        }

        // Print cache statistics
        auto cacheStats = hierarchy.getStats();
        for (size_t i = 0; i < cacheStats.size(); ++i) {
            auto [hitRate, hits, misses] = cacheStats[i];
            std::cout << "L" << (i+1) << " cache hit rate: " << (hitRate * 100) << "%" << std::endl;
            std::cout << "L" << (i+1) << " cache hits: " << hits << std::endl;
            std::cout << "L" << (i+1) << " cache misses: " << misses << std::endl;
        }

        // Print memory statistics
        std::cout << "Memory reads: " << memory->getReads() << std::endl;
        std::cout << "Memory writes: " << memory->getWrites() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}