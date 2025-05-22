// tests/visualize_cache.cpp
#include "metrics.hpp"
#include "json_config.hpp"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config1.json> [config2.json] ..." << std::endl;
        return 1;
    }
    
    try {
        PerformanceAnalyzer analyzer;
        
        // Load all test configurations
        for (int i = 1; i < argc; ++i) {
            std::string filename = argv[i];
            std::cout << "Loading configuration: " << filename << std::endl;
            auto config = JsonConfigLoader::loadTestConfig(filename);
            analyzer.addTest(std::move(config));
        }
        
        // Run all tests
        std::cout << "Running tests..." << std::endl;
        auto results = analyzer.runTests();
        
        // Compare results
        std::cout << "\nTest Results Comparison:" << std::endl;
        analyzer.compareResults(results);
        
        // Save results to CSV
        std::string csvFile = "cache_results.csv";
        analyzer.saveResultsToCSV(results, csvFile);
        
        // Generate visualization script
        std::string scriptFile = "visualize_results.py";
        analyzer.generateVisualizationScript(csvFile, scriptFile);
        
        std::cout << "\nTo generate visualizations, run: python3 " << scriptFile << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
