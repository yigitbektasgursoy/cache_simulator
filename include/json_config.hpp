// json_config.hpp
#pragma once

// Include necessary headers based on your project structure
#include "cache.hpp"    // Defines Cache, CacheConfig, CacheHierarchy, InclusionPolicy enum
#include "memory.hpp"   // Defines MainMemory, MemoryTraceSource, FileTraceSource, SyntheticTraceSource, MemoryAccess, AccessType enum
#include "metrics.hpp"  // Defines TestConfig, TestResult, PerformanceMetric, PerformanceAnalyzer
#include <nlohmann/json.hpp> // For JSON parsing
#include <fstream>           // For std::ifstream
#include <string>            // For std::string
#include <vector>            // For std::vector
#include <memory>            // For std::unique_ptr
#include <stdexcept>         // For std::runtime_error
#include <iostream>          // For std::cerr (warnings)

// Use JSON library by nlohmann
using json = nlohmann::json;

// Class to load test configurations from JSON
class JsonConfigLoader {
public:
    // Load a test configuration from a JSON file
    static std::unique_ptr<TestConfig> loadTestConfig(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("JsonConfigLoader: Could not open config file: " + filename);
        }

        json j;
        try {
            file >> j;
        } catch (json::parse_error& e) {
            throw std::runtime_error("JsonConfigLoader: JSON parse error in file '" + filename + "': " + e.what());
        }

        // Pass filename for better error context during parsing
        return parseTestConfigInternal(j, filename);
    }

private:
    // Internal helper to parse the main TestConfig object
    static std::unique_ptr<TestConfig> parseTestConfigInternal(const json& j, const std::string& config_filename) {
        // --- Parse Test Name (Required) ---
        std::string testName;
        if (j.contains("test_name") && j["test_name"].is_string()) {
            testName = j["test_name"].get<std::string>();
        } else {
            throw std::runtime_error("JsonConfigLoader: 'test_name' (string) is missing or invalid in '" + config_filename + "'.");
        }

        // --- Parse Cache Hierarchy (Required, must not be empty) ---
        std::vector<std::unique_ptr<Cache>> caches;
        if (j.contains("cache_hierarchy") && j["cache_hierarchy"].is_array()) {
            if (j["cache_hierarchy"].empty()) {
                throw std::runtime_error("JsonConfigLoader: 'cache_hierarchy' array cannot be empty in '" + config_filename + "'.");
            }
            for (const auto& cacheJson : j["cache_hierarchy"]) {
                if (!cacheJson.is_object()) {
                     throw std::runtime_error("JsonConfigLoader: Invalid item in 'cache_hierarchy' (must be an object) in '" + config_filename + "'.");
                }
                caches.push_back(parseCache(cacheJson, config_filename));
            }
        } else {
            throw std::runtime_error("JsonConfigLoader: 'cache_hierarchy' (array) is missing or invalid in '" + config_filename + "'.");
        }

        // --- Parse Memory Configuration (Required) ---
        std::unique_ptr<MainMemory> memory;
        if (j.contains("memory") && j["memory"].is_object()) {
            memory = parseMemory(j["memory"], config_filename);
        } else {
            throw std::runtime_error("JsonConfigLoader: 'memory' (object) is missing or invalid in '" + config_filename + "'.");
        }

        // --- Parse Trace Configuration (Required) ---
        std::unique_ptr<MemoryTraceSource> traceSource;
        if (j.contains("trace") && j["trace"].is_object()) {
            traceSource = parseTraceSource(j["trace"], config_filename);
        } else {
            throw std::runtime_error("JsonConfigLoader: 'trace' (object) is missing or invalid in '" + config_filename + "'.");
        }

        // Create and return the TestConfig object
        // The TestConfig constructor expects rvalue references for the unique_ptrs
        return std::make_unique<TestConfig>(
            std::move(testName),
            std::move(caches),     // std::vector<std::unique_ptr<Cache>>
            std::move(memory),     // std::unique_ptr<MainMemory>
            std::move(traceSource) // std::unique_ptr<MemoryTraceSource>
        );
    }

    // Parse an individual cache configuration from JSON
    static std::unique_ptr<Cache> parseCache(const json& j, const std::string& config_filename) {
        // CacheConfig has defaults, so we'll override them with JSON values
        CacheConfig config; // Instantiated with defaults from CacheConfig constructor

        // Helper to safely get a value of a specific type or throw
        auto get_json_field = [&](const std::string& key, const std::string& expected_type_str,
                                  auto& out_val, bool is_required = true) {
            if (j.contains(key)) {
                try {
                    out_val = j[key].get<std::decay_t<decltype(out_val)>>();
                } catch (const json::type_error& e) {
                    throw std::runtime_error("JsonConfigLoader: Cache field '" + key + "' has incorrect type (expected " +
                                             expected_type_str + ") in '" + config_filename + "'. Details: " + e.what());
                }
            } else if (is_required) {
                throw std::runtime_error("JsonConfigLoader: Missing required cache field '" + key + "' in '" + config_filename + "'.");
            }
            // If not required and not present, out_val retains its default from CacheConfig constructor
        };

        // --- Required fields (will override CacheConfig defaults) ---
        uint64_t parsed_level_u64; // CacheConfig::level is uint64_t in your struct
        get_json_field("level", "unsigned integer", parsed_level_u64);
        config.level = parsed_level_u64; // Assign to CacheConfig member

        std::string orgStr;
        get_json_field("organization", "string", orgStr);
        if (orgStr == "DirectMapped") {
            config.organization = CacheConfig::Organization::DirectMapped;
        } else if (orgStr == "SetAssociative") {
            config.organization = CacheConfig::Organization::SetAssociative;
        } else if (orgStr == "FullyAssociative") {
            config.organization = CacheConfig::Organization::FullyAssociative;
        } else {
            throw std::runtime_error("JsonConfigLoader: Unknown cache 'organization': '" + orgStr +
                                     "' for L" + std::to_string(config.level) + " in '" + config_filename + "'.");
        }

        get_json_field("size", "unsigned integer", config.size);
        get_json_field("block_size", "unsigned integer", config.blockSize);
        // config.associativity is uint64_t in CacheConfig
        uint64_t parsed_associativity_u64;
        get_json_field("associativity", "unsigned integer", parsed_associativity_u64);
        config.associativity = parsed_associativity_u64;


        get_json_field("policy", "string", config.policy); // Stored as string in CacheConfig
        // Validation of policy string happens when createPolicy is called in Cache constructor

        get_json_field("access_latency", "unsigned integer", config.accessLatency);
        get_json_field("write_back", "boolean", config.writeBack);
        get_json_field("write_allocate", "boolean", config.writeAllocate);

        // --- Optional field (will override CacheConfig default if present) ---
        if (j.contains("inclusion_policy")) {
            if (config.level >= 2) { // Your code applies this only for L2+
                std::string inclPolStr;
                get_json_field("inclusion_policy", "string", inclPolStr, false); // false = not required
                if (inclPolStr == "Inclusive") {
                    config.inclusionPolicy = InclusionPolicy::Inclusive;
                } else if (inclPolStr == "Exclusive") {
                    config.inclusionPolicy = InclusionPolicy::Exclusive;
                } else if (inclPolStr == "NINE") {
                    config.inclusionPolicy = InclusionPolicy::NINE;
                } else {
                    // Keep default from CacheConfig and warn
                    std::cerr << "Warning: Unknown 'inclusion_policy': '" << inclPolStr
                              << "' for L" << config.level << ". Using default ("
                              << static_cast<int>(config.inclusionPolicy) // Assuming default is set
                              << "). File: '" << config_filename << "'." << std::endl;
                }
            } else if (config.level == 1) {
                std::cerr << "Warning: 'inclusion_policy' specified for L1 cache (level " << config.level
                          << ") will be ignored. L1 defaults to Inclusive. File: '" << config_filename << "'." << std::endl;
                config.inclusionPolicy = InclusionPolicy::Inclusive; // Enforce default for L1
            }
        }


        return std::make_unique<Cache>(config);
    }

    // Parse memory configuration from JSON
    static std::unique_ptr<MainMemory> parseMemory(const json& j, const std::string& config_filename) {
        uint64_t accessLatency;
        if (j.contains("access_latency") && j["access_latency"].is_number_unsigned()) {
            accessLatency = j["access_latency"].get<uint64_t>();
        } else {
            throw std::runtime_error("JsonConfigLoader: 'memory.access_latency' (unsigned integer) is missing or invalid in '" + config_filename + "'.");
        }
        return std::make_unique<MainMemory>(accessLatency);
    }

    // Parse trace source configuration from JSON
    static std::unique_ptr<MemoryTraceSource> parseTraceSource(const json& j, const std::string& config_filename) {
        std::string type;
        if (j.contains("type") && j["type"].is_string()) {
            type = j["type"].get<std::string>();
        } else {
            throw std::runtime_error("JsonConfigLoader: 'trace.type' (string) is missing or invalid in '" + config_filename + "'.");
        }

        if (type == "File") {
            std::string filename;
            if (j.contains("filename") && j["filename"].is_string()) {
                filename = j["filename"].get<std::string>();
                if (filename.empty()){
                    throw std::runtime_error("JsonConfigLoader: 'trace.filename' cannot be empty for File trace type in '" + config_filename + "'.");
                }
            } else {
                throw std::runtime_error("JsonConfigLoader: 'trace.filename' (string) is missing or invalid for File trace type in '" + config_filename + "'.");
            }
            return std::make_unique<FileTraceSource>(filename);

        } else if (type == "Synthetic") {

            std::string patternStr;
            SyntheticTraceSource::Pattern pattern; // Enum from memory.hpp
            uint64_t startAddress, endAddress, numAccesses;
            double readRatio;

            // Helper for synthetic trace fields
            auto get_synth_field = [&](const std::string& key, const std::string& type_str, auto& out_val) {
                if (j.contains(key)) {
                     try { out_val = j[key].get<std::decay_t<decltype(out_val)>>(); }
                     catch (const json::type_error& e) {
                        throw std::runtime_error("JsonConfigLoader: Synthetic trace field '" + key + "' has incorrect type (expected " +
                                                 type_str + ") in '" + config_filename + "'. Details: " + e.what());
                     }
                } else {
                    throw std::runtime_error("JsonConfigLoader: Missing synthetic trace field '" + key + "' in '" + config_filename + "'.");
                }
            };

            get_synth_field("pattern", "string", patternStr);
            if (patternStr == "Sequential") pattern = SyntheticTraceSource::Pattern::Sequential;
            else if (patternStr == "Random") pattern = SyntheticTraceSource::Pattern::Random;
            else if (patternStr == "Strided") pattern = SyntheticTraceSource::Pattern::Strided;
            else if (patternStr == "Looping") pattern = SyntheticTraceSource::Pattern::Looping;
            else {
                throw std::runtime_error("JsonConfigLoader: Unknown 'trace.pattern': '" + patternStr + "' for Synthetic trace in '" + config_filename + "'.");
            }

            get_synth_field("start_address", "unsigned integer", startAddress);
            get_synth_field("end_address", "unsigned integer", endAddress);
            get_synth_field("num_accesses", "unsigned integer", numAccesses);
            get_synth_field("read_ratio", "number (double)", readRatio);

            if (readRatio < 0.0 || readRatio > 1.0) {
                throw std::runtime_error("JsonConfigLoader: 'trace.read_ratio' must be between 0.0 and 1.0 in '" + config_filename + "'.");
            }
            if (startAddress >= endAddress && (pattern == SyntheticTraceSource::Pattern::Sequential || pattern == SyntheticTraceSource::Pattern::Random || pattern == SyntheticTraceSource::Pattern::Strided) ) {
                 throw std::runtime_error("JsonConfigLoader: 'trace.start_address' must be less than 'trace.end_address' for most synthetic patterns in '" + config_filename + "'.");
            }


            return std::make_unique<SyntheticTraceSource>(pattern, startAddress, endAddress, numAccesses, readRatio);
        }
        else {
            throw std::runtime_error("JsonConfigLoader: Unknown 'trace.type': '" + type + "' in '" + config_filename + "'.");
        }
    }
};