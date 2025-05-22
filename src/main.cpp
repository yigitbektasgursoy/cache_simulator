// main.cpp - Cache Simulator Entry Point
#include "json_config.hpp"
#include "cache.hpp"
#include "memory.hpp"
#include "metrics.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

// Function to print cache statistics
void printCacheStats(const CacheHierarchy& hierarchy) {
    auto stats = hierarchy.getStats();

    std::cout << "\nCache Statistics:" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    for (size_t i = 0; i < stats.size(); ++i) {
        auto [hitRate, hits, misses] = stats[i];
        std::cout << "L" << (i + 1) << " Cache:" << std::endl;
        std::cout << "  Hits:   " << hits << std::endl;
        std::cout << "  Misses: " << misses << std::endl;
        std::cout << "  Total:  " << (hits + misses) << std::endl;
        std::cout << "  Hit Rate: " << std::fixed << std::setprecision(2)
                  << (hitRate * 100) << "%" << std::endl;

        // Get more details about the cache level
        const auto& cache = hierarchy.getCacheLevel(i);
        const auto& config = cache.getConfig();

        std::cout << "  Configuration:" << std::endl;
        std::cout << "    Size: " << config.size << " bytes" << std::endl;
        std::cout << "    Block Size: " << config.blockSize << " bytes" << std::endl;
        std::cout << "    Associativity: " << config.getNumWays() << " ways" << std::endl;
        std::cout << "    Sets: " << config.getNumSets() << std::endl;
        std::cout << "    Replacement Policy: " << config.policy << std::endl;

        if (i > 0) {
            std::string inclusionPolicy;
            switch (config.inclusionPolicy) {
                case InclusionPolicy::Inclusive:
                    inclusionPolicy = "Inclusive";
                    break;
                case InclusionPolicy::Exclusive:
                    inclusionPolicy = "Exclusive";
                    break;
                case InclusionPolicy::NINE:
                    inclusionPolicy = "Non-Inclusive Non-Exclusive";
                    break;
            }
            std::cout << "    Inclusion Policy: " << inclusionPolicy << std::endl;
        }

        std::cout << std::endl;
    }
}

// Function to print memory statistics
void printMemoryStats(const MainMemory& memory) {
    std::cout << "Memory Statistics:" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << "  Reads:  " << memory.getReads() << std::endl;
    std::cout << "  Writes: " << memory.getWrites() << std::endl;
    std::cout << "  Total:  " << memory.getAccesses() << std::endl;
    std::cout << "  Latency: " << memory.getAccessLatency() << " cycles" << std::endl;
    std::cout << std::endl;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <config1.json> [config2.json ...] [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --visualize    Generate visualization scripts for results" << std::endl;
    std::cout << "  --compare      Compare results across multiple configurations" << std::endl;
    std::cout << "  --verbose      Display detailed output" << std::endl;
    std::cout << "  --help         Display this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> configFiles;
    bool generateVisualizations = true;
    bool compareConfigs = false;
    bool verbose = false;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-') {
            // This is an option
            if (arg == "--visualize") {
                generateVisualizations = true;
            } else if (arg == "--compare") {
                compareConfigs = true;
            } else if (arg == "--verbose") {
                verbose = true;
            } else if (arg == "--help") {
                printUsage(argv[0]);
                return 0;
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else {
            // This is a configuration file
            configFiles.push_back(arg);
        }
    }

    if (configFiles.empty()) {
        std::cerr << "Error: No configuration files specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Check if all config files exist
    for (const auto& file : configFiles) {
        if (!std::filesystem::exists(file)) {
            std::cerr << "Error: Configuration file not found: " << file << std::endl;
            return 1;
        }
    }

    std::cout << "Cache Simulator" << std::endl;
    std::cout << "==============" << std::endl;

    // Handle comparison mode
    if (compareConfigs || configFiles.size() > 1) {
        PerformanceAnalyzer analyzer;

        // Load all test configurations
        for (const auto& file : configFiles) {
            std::cout << "Loading configuration: " << file << std::endl;
            auto config = JsonConfigLoader::loadTestConfig(file);
            analyzer.addTest(std::move(config));
        }

        // Run all tests
        std::cout << "Running simulations..." << std::endl;
        auto results = analyzer.runTests();

        // Compare results
        std::cout << "\nResults Comparison:" << std::endl;
        analyzer.compareResults(results);

        // Save results to CSV and generate visualization if requested
        if (generateVisualizations) {
            std::string csvFile = "cache_comparison_results.csv";
            analyzer.saveResultsToCSV(results, csvFile);

            std::cout << "\nGenerated visualization files:" << std::endl;
            std::cout << "  " << csvFile << std::endl;
        }

        return 0;
    }

    // Handle single configuration mode
    // Run the simulation with the first config file only
    try {
        const std::string& configFile = configFiles[0];
        std::cout << "Running simulation with: " << configFile << std::endl;

        // Load the configuration
        auto testConfig = JsonConfigLoader::loadTestConfig(configFile);
        std::cout << "Test name: " << testConfig->name << std::endl;

        // Setup cache hierarchy
        CacheHierarchy hierarchy;
        for (const auto& cache : testConfig->caches) {
            hierarchy.addCacheLevel(std::make_unique<Cache>(*cache));
        }

        std::cout << "Cache hierarchy created with " << testConfig->caches.size() << " levels" << std::endl;

        // Get a reference to the memory
        auto& memory = *(testConfig->memory);

        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();

        // Process the trace
        auto& traceSource = testConfig->traceSource;
        uint64_t totalLatency = 0;
        uint64_t accessCount = 0;
        uint64_t hitCount = 0;

        std::cout << "Processing memory trace..." << std::endl;

        // Process all memory accesses
        while (auto access = traceSource->getNextAccess()) {
            if (verbose && accessCount % 100000 == 0) {
                std::cout << "Processed " << accessCount << " accesses..." << std::endl;
            }

            // Access the cache hierarchy
            auto [latency, hitInCache] = hierarchy.access(access->address, access->type);

            // If cache miss at all levels, access main memory
            if (!hitInCache) {
                latency += memory.access(access->address, access->type);
            } else {
                hitCount++;
            }

            totalLatency += latency;
            accessCount++;
        }

        // Stop timing
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Print results
        std::cout << "\nSimulation completed in " << duration.count() << " ms" << std::endl;
        std::cout << "Processed " << accessCount << " memory accesses" << std::endl;

        if (accessCount > 0) {
            double overallHitRate = static_cast<double>(hitCount) / accessCount * 100;
            double avgLatency = static_cast<double>(totalLatency) / accessCount;

            std::cout << "Overall hit rate: " << std::fixed << std::setprecision(2)
                      << overallHitRate << "%" << std::endl;
            std::cout << "Average memory access time: " << std::fixed << std::setprecision(2)
                      << avgLatency << " cycles" << std::endl;
        }

        // Print detailed statistics
        printCacheStats(hierarchy);
        printMemoryStats(memory);

        // Generate visualizations if requested
        if (generateVisualizations) {
            // Create a performance analyzer
            PerformanceAnalyzer analyzer;

            // Add the current test configuration
            std::vector<std::unique_ptr<Cache>> cachesCopy;
            for (const auto& cache : testConfig->caches) {
                cachesCopy.push_back(std::make_unique<Cache>(*cache));
            }

            // Reset the trace source
            testConfig->traceSource->reset();

            analyzer.addTest(std::make_unique<TestConfig>(
                testConfig->name,
                std::move(cachesCopy),
                std::make_unique<MainMemory>(memory.getAccessLatency()),
                testConfig->traceSource->clone()
            ));

            // Run the test
            auto results = analyzer.runTests();

            // Save results to CSV
            std::string csvFile = testConfig->name + "_results.csv";
            analyzer.saveResultsToCSV(results, csvFile);

            std::cout << "\nGenerated visualization files:" << std::endl;
            std::cout << "  " << csvFile << std::endl;

        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}