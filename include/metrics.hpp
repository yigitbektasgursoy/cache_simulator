// metrics.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <format>
#include <filesystem>

// Forward declarations
class Cache;
class CacheHierarchy;
class MainMemory;
class MemoryTraceSource;

// Performance metric
struct PerformanceMetric {
    std::string name;
    double value;
    std::string unit;
    
    PerformanceMetric(std::string n, double v, std::string u)
        : name(std::move(n)), value(v), unit(std::move(u)) {}
};

// Test configuration
struct TestConfig {
    std::string name;
    std::vector<std::unique_ptr<Cache>> caches;
    std::unique_ptr<MainMemory> memory;
    std::unique_ptr<MemoryTraceSource> traceSource;
    
    TestConfig(std::string n, 
              std::vector<std::unique_ptr<Cache>> c,
              std::unique_ptr<MainMemory> m,
              std::unique_ptr<MemoryTraceSource> t)
        : name(std::move(n)),
          caches(std::move(c)),
          memory(std::move(m)),
          traceSource(std::move(t)) {}
};

// Test result
struct TestResult {
    std::string testName;
    std::chrono::microseconds executionTime;
    std::vector<PerformanceMetric> metrics;
    
    TestResult(std::string name, std::chrono::microseconds time)
        : testName(std::move(name)), executionTime(time) {}
        
    void addMetric(const std::string& name, double value, const std::string& unit) {
        metrics.emplace_back(name, value, unit);
    }
};

// Performance analyzer
class PerformanceAnalyzer {
public:
    // Add a test configuration
    void addTest(std::unique_ptr<TestConfig> config);
    
    // Run all tests and collect metrics
    std::vector<TestResult> runTests();
    
    // Compare test results
    void compareResults(const std::vector<TestResult>& results) const;
    
    // Save results to a CSV file
    void saveResultsToCSV(const std::vector<TestResult>& results, const std::string& filename) const;

private:
    std::vector<std::unique_ptr<TestConfig>> testConfigs_;
    
    // Collect metrics from a cache hierarchy
    std::vector<PerformanceMetric> collectMetrics(const CacheHierarchy& hierarchy, 
                                                 const MainMemory& memory,
                                                 std::chrono::microseconds executionTime) const;
};
