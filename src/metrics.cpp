// metrics.cpp
#include "metrics.hpp"
#include "cache.hpp"
#include "memory.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>

void PerformanceAnalyzer::addTest(std::unique_ptr<TestConfig> config) {
    testConfigs_.push_back(std::move(config));
}

std::vector<TestResult> PerformanceAnalyzer::runTests() {
    std::vector<TestResult> results;
    
    for (const auto& config : testConfigs_) {
        std::cout << "Running test: " << config->name << std::endl;
        
        // Setup cache hierarchy
        CacheHierarchy hierarchy;
        for (const auto& cache : config->caches) {
            hierarchy.addCacheLevel(std::make_unique<Cache>(*cache));
        }
        
        // Reset memory and trace source
        config->memory->reset();
        config->traceSource->reset();
        
        // Run the test and measure time
        auto start = std::chrono::high_resolution_clock::now();
        
        uint64_t totalLatency = 0;
        uint64_t accessCount = 0;
        
        // Process all memory accesses
        while (auto access = config->traceSource->getNextAccess()) {
            // Access the cache hierarchy
            auto [latency, hitInCache] = hierarchy.access(access->address, access->type);

            // If cache miss at all levels, access main memory
            if (!hitInCache) {
                latency += config->memory->access(access->address, access->type);
            }
            
            totalLatency += latency;
            accessCount++;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Collect results
        TestResult result(config->name, duration);
        
        // Add average memory access time
        if (accessCount > 0) {
            result.addMetric("Average Access Time", 
                           static_cast<double>(totalLatency) / accessCount, 
                           "cycles");
        }
        
        // Collect metrics from the cache hierarchy and memory
        std::vector<PerformanceMetric> metrics = 
            collectMetrics(hierarchy, *config->memory, duration);
        
        // Add metrics to result
        for (const auto& metric : metrics) {
            result.addMetric(metric.name, metric.value, metric.unit);
        }
        
        results.push_back(result);
    }
    
    return results;
}

std::vector<PerformanceMetric> PerformanceAnalyzer::collectMetrics(
    const CacheHierarchy& hierarchy,
    const MainMemory& memory,
    std::chrono::microseconds executionTime) const {

    std::vector<PerformanceMetric> metrics;

    // Get stats for each cache level
    auto cacheStats = hierarchy.getStats();

    //
    // AMAT CALCULATION - CORRECTED IMPLEMENTATION
    //

    // Always pay the full L1 access latency (all requests go through L1)
    double l1Latency = hierarchy.getCacheLevel(0).getConfig().accessLatency;
    auto [l1HitRate, l1Hits, l1Misses] = cacheStats[0];
    double l1MissRate = 1.0 - l1HitRate;

    // Base AMAT starts with L1 latency
    double totalAMAT = l1Latency;

    // Store L1 latency contribution
    metrics.emplace_back(
        "L1 AMAT Contribution",
        l1Latency,
        "cycles"
    );

    // Running probability of reaching each level
    double missPathProbability = l1MissRate;

    // Add each cache level's contribution to AMAT (L2+)
    for (size_t i = 1; i < cacheStats.size(); ++i) {
        auto [hitRate, hits, misses] = cacheStats[i];
        double missRate = 1.0 - hitRate;
        double accessLatency = hierarchy.getCacheLevel(i).getConfig().accessLatency;

        // This level's contribution to AMAT is its latency multiplied by the
        // probability of reaching this level (all previous misses)
        double levelContribution = missPathProbability * accessLatency;
        totalAMAT += levelContribution;

        // Store this level's contribution
        metrics.emplace_back(
            "L" + std::to_string(i + 1) + " AMAT Contribution",
            levelContribution,
            "cycles"
        );

        // Update probability for the next level (current level miss probability)
        missPathProbability *= missRate;
    }

    // Add memory contribution - only when all caches miss
    double memoryContribution = missPathProbability * memory.getAccessLatency();
    totalAMAT += memoryContribution;

    // Add memory contribution (optional - for visibility)
    metrics.emplace_back(
        "Memory AMAT Contribution",
        memoryContribution,
        "cycles"
    );

    // Add the total AMAT
    metrics.emplace_back(
        "Total System AMAT",
        totalAMAT,
        "cycles"
    );

    //
    // HIT RATE STATISTICS
    //

    // Add hit rate statistics for each cache level
    for (size_t i = 0; i < cacheStats.size(); ++i) {
        auto [hitRate, hits, misses] = cacheStats[i];

        // Add hit rate
        metrics.emplace_back(
            "L" + std::to_string(i + 1) + " Hit Rate",
            hitRate,
            "%"
        );

        // Add hits and misses
        metrics.emplace_back(
            "L" + std::to_string(i + 1) + " Hits",
            static_cast<double>(hits),
            "accesses"
        );

        metrics.emplace_back(
            "L" + std::to_string(i + 1) + " Misses",
            static_cast<double>(misses),
            "accesses"
        );
    }

    //
    // INCLUSION POLICY INFORMATION
    //

    // Add inclusion policy information for L2+ caches
    for (size_t i = 1; i < cacheStats.size(); ++i) {
        const auto& cache = hierarchy.getCacheLevel(i);
        std::string policyStr;

        switch(cache.getConfig().inclusionPolicy) {
            case InclusionPolicy::Inclusive:
                policyStr = "Inclusive";
                break;
            case InclusionPolicy::Exclusive:
                policyStr = "Exclusive";
                break;
            case InclusionPolicy::NINE:
                policyStr = "NINE";
                break;
        }

        // Store string directly as unit to avoid numeric display
        metrics.emplace_back(
            "L" + std::to_string(i + 1) + " Inclusion Policy",
            1.0,  // Just a placeholder value that won't be displayed
            policyStr  // The policy name will be displayed in the unit column
        );
    }

    //
    // MEMORY ACCESS STATISTICS
    //

    // Add memory access stats
    metrics.emplace_back(
        "Memory Reads",
        static_cast<double>(memory.getReads()),
        "accesses"
    );

    metrics.emplace_back(
        "Memory Writes",
        static_cast<double>(memory.getWrites()),
        "accesses"
    );

    return metrics;
}

void PerformanceAnalyzer::compareResults(const std::vector<TestResult>& results) const {
    if (results.empty()) {
        std::cout << "No results to compare" << std::endl;
        return;
    }

    // Find all metric names across all results
    std::vector<std::string> metricNames;
    for (const auto& result : results) {
        for (const auto& metric : result.metrics) {
            if (std::find(metricNames.begin(), metricNames.end(), metric.name) == metricNames.end()) {
                metricNames.push_back(metric.name);
            }
        }
    }

    // Calculate the optimal width for the metric column (min 20 chars)
    size_t metricColWidth = 20;
    for (const auto& name : metricNames) {
        metricColWidth = std::max(metricColWidth, name.length() + 2);
    }

    // Calculate the optimal width for each result column (min 15 chars)
    std::vector<size_t> resultColWidths;
    for (const auto& result : results) {
        size_t colWidth = std::max(result.testName.length() + 2, static_cast<size_t>(15));

        // Check if any metric value needs more space
        for (const auto& metricName : metricNames) {
            for (const auto& metric : result.metrics) {
                if (metric.name == metricName) {
                    std::stringstream valueStr;

                    // Format the value with appropriate precision
                    if (metric.name.find("Hit Rate") != std::string::npos) {
                        valueStr << std::fixed << std::setprecision(2)
                                << (metric.value * 100) << " " << metric.unit;
                    } else {
                        valueStr << std::fixed << std::setprecision(2)
                                << metric.value << " " << metric.unit;
                    }

                    colWidth = std::max(colWidth, valueStr.str().length() + 2);
                    break;
                }
            }
        }

        resultColWidths.push_back(colWidth);
    }

    // Print header row
    std::cout << std::left << std::setw(metricColWidth) << "Metric";
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << " | " << std::left << std::setw(resultColWidths[i]) << results[i].testName;
    }
    std::cout << std::endl;

    // Print separator line
    std::cout << std::string(metricColWidth, '-');
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "-+-" << std::string(resultColWidths[i], '-');
    }
    std::cout << std::endl;

    // Print metric rows
    for (const auto& metricName : metricNames) {
        std::cout << std::left << std::setw(metricColWidth) << metricName;

        for (size_t i = 0; i < results.size(); ++i) {
            const auto& result = results[i];
            bool found = false;

            for (const auto& metric : result.metrics) {
                if (metric.name == metricName) {
                    std::stringstream valueStr;

                    // Format value based on the metric type
                    if (metricName.find("Inclusion Policy") != std::string::npos) {
                        // Special handling for inclusion policy - show only the string
                        valueStr << metric.unit;
                    } else if (metric.name.find("Hit Rate") != std::string::npos) {
                        valueStr << std::fixed << std::setprecision(2)
                                << (metric.value * 100) << " " << metric.unit;
                    } else {
                        valueStr << std::fixed << std::setprecision(2)
                                << metric.value << " " << metric.unit;
                    }

                    std::cout << " | " << std::left << std::setw(resultColWidths[i]) << valueStr.str();
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::cout << " | " << std::left << std::setw(resultColWidths[i]) << "N/A";
            }
        }

        std::cout << std::endl;
    }
}

void PerformanceAnalyzer::saveResultsToCSV(const std::vector<TestResult>& results, const std::string& filename) const {
    if (results.empty()) {
        std::cout << "No results to save" << std::endl;
        return;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }

    // Find all metric names across all results
    std::vector<std::string> metricNames;
    for (const auto& result : results) {
        for (const auto& metric : result.metrics) {
            if (std::find(metricNames.begin(), metricNames.end(), metric.name) == metricNames.end()) {
                metricNames.push_back(metric.name);
            }
        }
    }

    // Write header
    file << "Metric";
    for (const auto& result : results) {
        file << "," << result.testName;
    }
    file << std::endl;

    // Write metrics
    for (const auto& metricName : metricNames) {
        file << metricName;

        for (const auto& result : results) {
            bool found = false;
            for (const auto& metric : result.metrics) {
                if (metric.name == metricName) {
                    if (metricName.find("Inclusion Policy") != std::string::npos) {
                        // For inclusion policy, write the policy name
                        file << "," << metric.unit;
                    } else if (metric.name.find("Hit Rate") != std::string::npos) {
                        file << "," << (metric.value * 100); // Multiply by 100 for percentages
                    } else {
                        file << "," << metric.value;
                    }
                    found = true;
                    break;
                }
            }

            if (!found) {
                file << ",";
            }
        }

        file << std::endl;
    }

    file.close();
    std::cout << "Results saved to " << filename << std::endl;
}
