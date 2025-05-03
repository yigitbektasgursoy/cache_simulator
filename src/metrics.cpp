// metrics.cpp
#include "metrics.hpp"
#include "cache.hpp"
#include "memory.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>

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
            uint64_t latency = hierarchy.access(access->address, access->type);
            
            // If cache miss, access main memory
            if (latency == 0) {
                latency = config->memory->access(access->address, access->type);
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
    
    // Add execution time
    metrics.emplace_back(
        "Execution Time", 
        static_cast<double>(executionTime.count()), 
        "us"
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
    
    // Print header
    std::cout << std::setw(30) << "Metric";
    for (const auto& result : results) {
        std::cout << " | " << std::setw(15) << result.testName;
    }
    std::cout << std::endl;
    
    std::cout << std::string(30, '-');
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "-+-" << std::string(15, '-');
    }
    std::cout << std::endl;
    
    // Print metrics
    for (const auto& metricName : metricNames) {
        std::cout << std::setw(30) << metricName;
        
        for (const auto& result : results) {
            bool found = false;
            for (const auto& metric : result.metrics) {
                if (metric.name == metricName) {
                    std::cout << " | " << std::setw(13) << std::fixed << std::setprecision(2) 
                             << metric.value << " " << metric.unit;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                std::cout << " | " << std::setw(15) << "N/A";
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
                    file << "," << metric.value;
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

void PerformanceAnalyzer::exportForPlotting(const std::vector<TestResult>& results, const std::string& directory) const {
    // Create directory if it doesn't exist
    std::filesystem::create_directories(directory);
    
    // Save results as CSV
    saveResultsToCSV(results, directory + "/results.csv");
    
    // Generate Python script for plotting
    std::ofstream pyFile(directory + "/plot_results.py");
    if (!pyFile.is_open()) {
        std::cerr << "Could not open file: " << directory << "/plot_results.py" << std::endl;
        return;
    }
    
    pyFile << R"(
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Read results
results = pd.read_csv('results.csv')

# Set up the figure size
plt.figure(figsize=(12, 8))

# Find metrics
metrics = results['Metric'].tolist()
test_configs = results.columns[1:]

# Create a bar chart for each metric
for i, metric in enumerate(metrics):
    metric_data = results.iloc[i, 1:].values
    
    # Skip if all values are NaN
    if np.isnan(metric_data).all():
        continue
    
    plt.figure(figsize=(10, 6))
    bars = plt.bar(test_configs, metric_data)
    plt.title(f'{metric} Comparison')
    plt.ylabel(metric)
    plt.xticks(rotation=45, ha='right')
    
    # Add values on top of bars
    for bar in bars:
        height = bar.get_height()
        if not np.isnan(height):
            plt.text(bar.get_x() + bar.get_width()/2., height,
                    f'{height:.2f}',
                    ha='center', va='bottom')
    
    plt.tight_layout()
    plt.savefig(f'{metric.replace(" ", "_")}.png')
    plt.close()

print("Plots saved to current directory")
)";
    
    pyFile.close();
    std::cout << "Python script for plotting saved to " << directory << "/plot_results.py" << std::endl;
}
