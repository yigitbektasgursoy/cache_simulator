// Amalgamated on Tue Apr 29 03:33:44 PM +03 2025

// --- include/address.hpp ---
// address.hpp
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <bitset>
#include <concepts>
#include <format>

// C++20 concept to ensure numeric type
template<typename T>
concept NumericType = std::is_integral_v<T>;

class MemoryAddress {
public:
    // Constructor with type-constrained template using concepts
    template<NumericType T>
    explicit MemoryAddress(T address) : address_(static_cast<uint64_t>(address)) {}

    // Get the full address
    [[nodiscard]] uint64_t getAddress() const { return address_; }

    // Extract bit field from address (from position start to end inclusive)
    [[nodiscard]] uint64_t getBits(uint8_t start, uint8_t end) const;
    
    // Get tag bits for a specific cache configuration
    [[nodiscard]] uint64_t getTag(uint8_t blockOffsetBits, uint8_t indexBits) const;
    
    // Get index bits for a specific cache configuration
    [[nodiscard]] uint64_t getIndex(uint8_t blockOffsetBits, uint8_t indexBits) const;
    
    // Get block offset bits
    [[nodiscard]] uint64_t getBlockOffset(uint8_t blockOffsetBits) const;
    
    // String representation
    [[nodiscard]] std::string toString() const;
    
    // Equality operators
    bool operator==(const MemoryAddress& other) const { return address_ == other.address_; }
    bool operator!=(const MemoryAddress& other) const { return address_ != other.address_; }
    
private:
    uint64_t address_;
};

// Hash function for MemoryAddress to use in unordered containers
namespace std {
    template<>
    struct hash<MemoryAddress> {
        std::size_t operator()(const MemoryAddress& address) const {
            return std::hash<uint64_t>()(address.getAddress());
        }
    };
}
// --- include/cache.hpp ---
// cache.hpp
#pragma once

#include "address.hpp"
#include "cache_policy.hpp"
#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <variant>
#include <span>
#include <unordered_map>

// Cache entry (block) in the cache
class CacheEntry {
public:
    CacheEntry() : valid_(false), dirty_(false), tag_(0) {}
    
    void reset() {
        valid_ = false;
        dirty_ = false;
        tag_ = 0;
    }
    
    [[nodiscard]] bool isValid() const { return valid_; }
    void setValid(bool valid) { valid_ = valid; }
    
    [[nodiscard]] bool isDirty() const { return dirty_; }
    void setDirty(bool dirty) { dirty_ = dirty; }
    
    [[nodiscard]] uint64_t getTag() const { return tag_; }
    void setTag(uint64_t tag) { tag_ = tag; }

private:
    bool valid_;   // Is the entry valid
    bool dirty_;   // Has the entry been modified (for write-back caches)
    uint64_t tag_; // Tag bits from the address
};

// Cache configuration parameters
struct CacheConfig {
    enum class Organization {
        DirectMapped,
        SetAssociative,
        FullyAssociative
    };
    
    Organization organization;
    uint64_t size;         // Total size in bytes
    uint64_t blockSize;    // Size of each block in bytes
    uint64_t associativity; // Number of ways (meaningful for set-associative)
    std::string policy;     // Replacement policy name
    uint64_t accessLatency; // Cycles to access this cache
    bool writeBack;         // Write-back vs write-through
    bool writeAllocate;     // Write-allocate vs no-write-allocate
    
    // Constructor with default values
    CacheConfig() :
        organization(Organization::SetAssociative),
        size(65536),        // 64KB default
        blockSize(64),      // 64B blocks
        associativity(8),   // 8-way set associative
        policy("LRU"),
        accessLatency(1),   // 1 cycle latency
        writeBack(true),    // Default to write-back
        writeAllocate(true) // Default to write-allocate
    {}
    
    // Calculate number of sets
    [[nodiscard]] uint64_t getNumSets() const {
        if (organization == Organization::FullyAssociative) {
            return 1;
        } else if (organization == Organization::DirectMapped) {
            return size / blockSize;
        } else {
            return size / (blockSize * associativity);
        }
    }
    
    // Calculate number of ways
    [[nodiscard]] uint64_t getNumWays() const {
        if (organization == Organization::FullyAssociative) {
            return size / blockSize;
        } else if (organization == Organization::DirectMapped) {
            return 1;
        } else {
            return associativity;
        }
    }
    
    // Calculate block offset bits
    [[nodiscard]] uint8_t getBlockOffsetBits() const {
        // Determine how many bits are needed to address within a block
        uint64_t count = 0;
        uint64_t temp = blockSize;
        while (temp > 1) {
            temp >>= 1;
            count++;
        }
        return static_cast<uint8_t>(count);
    }
    
    // Calculate index bits
    [[nodiscard]] uint8_t getIndexBits() const {
        // For fully associative, there's no index
        if (organization == Organization::FullyAssociative) {
            return 0;
        }
        
        uint64_t numSets = getNumSets();
        uint64_t count = 0;
        while (numSets > 1) {
            numSets >>= 1;
            count++;
        }
        return static_cast<uint8_t>(count);
    }
};

// Memory access type
enum class AccessType {
    Read,
    Write
};

// Cache access result
struct CacheResult {
    bool hit;
    uint64_t latency;
    bool writeBack;  // Whether a write-back was triggered
    std::optional<MemoryAddress> evictedAddress; // Address of the block evicted (if any)
};

// Generic cache class
class Cache {
public:
    explicit Cache(const CacheConfig& config);
    
    // Copy constructor (used when cloning cache configurations)
    Cache(const Cache& other);
    
    // Access the cache with a memory address (either read or write)
    [[nodiscard]] CacheResult access(const MemoryAddress& address, AccessType type);
    
    // Get statistics
    [[nodiscard]] uint64_t getHits() const { return hits_; }
    [[nodiscard]] uint64_t getMisses() const { return misses_; }
    [[nodiscard]] uint64_t getAccesses() const { return hits_ + misses_; }
    [[nodiscard]] double getHitRate() const {
        return getAccesses() > 0 ? static_cast<double>(hits_) / getAccesses() : 0.0;
    }
    
    // Reset the cache and statistics
    void reset();
    
    // Get configuration
    [[nodiscard]] const CacheConfig& getConfig() const { return config_; }

private:
    CacheConfig config_;
    std::unique_ptr<ReplacementPolicy> policy_;
    
    // Cache structure: vector of sets, each set is a vector of ways (entries)
    std::vector<std::vector<CacheEntry>> cacheEntries_;
    
    // Performance statistics
    uint64_t hits_;
    uint64_t misses_;
    
    // Calculate set index and tag for an address
    [[nodiscard]] std::pair<uint64_t, uint64_t> getSetAndTag(const MemoryAddress& address) const;
    
    // Find an entry in the cache
    [[nodiscard]] std::optional<uint64_t> findEntry(uint64_t set, uint64_t tag) const;
    
    // Allocate a new entry in the cache
    [[nodiscard]] CacheResult allocateEntry(uint64_t set, uint64_t tag, AccessType type);
};

// Multi-level cache hierarchy
class CacheHierarchy {
public:
    // Add a cache level to the hierarchy
    void addCacheLevel(std::unique_ptr<Cache> cache);
    
    // Access the memory through all cache levels
    [[nodiscard]] uint64_t access(const MemoryAddress& address, AccessType type);
    
    // Reset all caches in the hierarchy
    void reset();
    
    // Get statistics for all cache levels
    [[nodiscard]] std::vector<std::tuple<double, uint64_t, uint64_t>> getStats() const;

private:
    std::vector<std::unique_ptr<Cache>> caches_;
};
// --- include/cache_policy.hpp ---
// cache_policy.hpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <list>
#include <unordered_map>
#include <random>
#include <functional>
#include <ranges>
#include <algorithm>

// Forward declaration
class CacheEntry;

// Base policy interface
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    
    // Called when an entry is accessed
    virtual void onAccess(uint64_t set, uint64_t way) = 0;
    
    // Select a way to replace within a set
    [[nodiscard]] virtual uint64_t getVictim(uint64_t set, uint64_t numWays) = 0;
    
    // Clone the policy (for copying cache configurations)
    [[nodiscard]] virtual std::unique_ptr<ReplacementPolicy> clone() const = 0;
};

// LRU (Least Recently Used) policy
class LRUPolicy : public ReplacementPolicy {
public:
    LRUPolicy() = default;
    
    void onAccess(uint64_t set, uint64_t way) override;
    [[nodiscard]] uint64_t getVictim(uint64_t set, uint64_t numWays) override;
    [[nodiscard]] std::unique_ptr<ReplacementPolicy> clone() const override;

private:
    // Map from set index to list of ways ordered by recency of use
    std::unordered_map<uint64_t, std::list<uint64_t>> lruList_;
    
    // Quick lookup for position in the LRU list
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::list<uint64_t>::iterator>> lruPositions_;
};

// FIFO (First In First Out) policy
class FIFOPolicy : public ReplacementPolicy {
public:
    FIFOPolicy() = default;
    
    void onAccess(uint64_t set, uint64_t way) override;
    [[nodiscard]] uint64_t getVictim(uint64_t set, uint64_t numWays) override;
    [[nodiscard]] std::unique_ptr<ReplacementPolicy> clone() const override;

private:
    // Map from set index to queue of ways ordered by insertion time
    std::unordered_map<uint64_t, std::list<uint64_t>> fifoQueue_;
};

// Random replacement policy
class RandomPolicy : public ReplacementPolicy {
public:
    RandomPolicy() : rng_(std::random_device{}()) {}
    
    void onAccess(uint64_t set, uint64_t way) override;
    [[nodiscard]] uint64_t getVictim(uint64_t set, uint64_t numWays) override;
    [[nodiscard]] std::unique_ptr<ReplacementPolicy> clone() const override;

private:
    std::mt19937 rng_;
};

// Factory function to create policy by name - demonstrates modern C++ function objects
[[nodiscard]] inline std::unique_ptr<ReplacementPolicy> createPolicy(const std::string& name) {
    static const std::unordered_map<std::string, std::function<std::unique_ptr<ReplacementPolicy>()>> policyMap = {
        {"LRU", []() { return std::make_unique<LRUPolicy>(); }},
        {"FIFO", []() { return std::make_unique<FIFOPolicy>(); }},
        {"RANDOM", []() { return std::make_unique<RandomPolicy>(); }}
    };
    
    if (auto it = policyMap.find(name); it != policyMap.end()) {
        return it->second();
    }
    
    // Default to LRU
    return std::make_unique<LRUPolicy>();
}
// --- include/memory.hpp ---
// memory.hpp
#pragma once

#include "address.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <variant>
#include <optional>
#include <functional>
#include <random>

// Forward declaration
enum class AccessType;

// Memory trace entry
struct MemoryAccess {
    MemoryAddress address;
    AccessType type;
    uint64_t accessTime;  // In CPU cycles
    
    MemoryAccess(MemoryAddress addr, AccessType t, uint64_t time = 0)
        : address(addr), type(t), accessTime(time) {}
};

// Memory trace sources
class MemoryTraceSource {
public:
    virtual ~MemoryTraceSource() = default;
    
    // Get the next memory access in the trace
    [[nodiscard]] virtual std::optional<MemoryAccess> getNextAccess() = 0;
    
    // Reset the trace to the beginning
    virtual void reset() = 0;
    
    // Clone the trace source
    [[nodiscard]] virtual std::unique_ptr<MemoryTraceSource> clone() const = 0;
};

// File-based trace source (e.g., reading from a trace file)
class FileTraceSource : public MemoryTraceSource {
public:
    explicit FileTraceSource(const std::string& filename);
    
    [[nodiscard]] std::optional<MemoryAccess> getNextAccess() override;
    void reset() override;
    [[nodiscard]] std::unique_ptr<MemoryTraceSource> clone() const override;

private:
    std::string filename_;
    std::ifstream file_;
    uint64_t lineNumber_;
};

// Synthetic trace generator (generates access patterns algorithmically)
class SyntheticTraceSource : public MemoryTraceSource {
public:
    enum class Pattern {
        Sequential,    // Sequential memory accesses
        Random,        // Random memory accesses
        Strided,       // Strided access pattern (e.g., matrix access)
        Looping        // Repeated access to a small set of addresses
    };
    
    SyntheticTraceSource(Pattern pattern, 
                        uint64_t startAddress, 
                        uint64_t endAddress,
                        uint64_t numAccesses,
                        double readRatio = 0.7);
    
    [[nodiscard]] std::optional<MemoryAccess> getNextAccess() override;
    void reset() override;
    [[nodiscard]] std::unique_ptr<MemoryTraceSource> clone() const override;

private:
    Pattern pattern_;
    uint64_t startAddress_;
    uint64_t endAddress_;
    uint64_t numAccesses_;
    double readRatio_;
    
    // Current state
    uint64_t currentAccess_;
    std::mt19937 rng_;
    
    // For looping pattern
    std::vector<uint64_t> loopAddresses_;
    
    // Generate an address based on the pattern
    [[nodiscard]] uint64_t generateAddress();
    
    // Generate an access type (read or write)
    [[nodiscard]] AccessType generateAccessType();
};

// Function-based trace source (allows custom access pattern generation)
class FunctionTraceSource : public MemoryTraceSource {
public:
    using GeneratorFunction = std::function<std::optional<MemoryAccess>()>;
    using ResetFunction = std::function<void()>;
    
    FunctionTraceSource(GeneratorFunction generator, ResetFunction reset);
    
    [[nodiscard]] std::optional<MemoryAccess> getNextAccess() override;
    void reset() override;
    [[nodiscard]] std::unique_ptr<MemoryTraceSource> clone() const override;

private:
    GeneratorFunction generator_;
    ResetFunction reset_;
    bool clonable_;
};

// Main memory model (sits below the cache hierarchy)
class MainMemory {
public:
    explicit MainMemory(uint64_t accessLatency = 100);
    
    // Access the main memory
    [[nodiscard]] uint64_t access(const MemoryAddress& address, AccessType type);
    
    // Get statistics
    [[nodiscard]] uint64_t getReads() const { return reads_; }
    [[nodiscard]] uint64_t getWrites() const { return writes_; }
    [[nodiscard]] uint64_t getAccesses() const { return reads_ + writes_; }
    
    // Reset statistics
    void reset();
    
    // Set access latency
    void setAccessLatency(uint64_t latency) { accessLatency_ = latency; }
    [[nodiscard]] uint64_t getAccessLatency() const { return accessLatency_; }

private:
    uint64_t accessLatency_;
    uint64_t reads_;
    uint64_t writes_;
};
// --- include/metrics.hpp ---
// metrics.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <sstream>
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
    
    // Export results to a format suitable for plotting (e.g., with Python)
    void exportForPlotting(const std::vector<TestResult>& results, const std::string& directory) const;

private:
    std::vector<std::unique_ptr<TestConfig>> testConfigs_;
    
    // Collect metrics from a cache hierarchy
    std::vector<PerformanceMetric> collectMetrics(const CacheHierarchy& hierarchy, 
                                                 const MainMemory& memory,
                                                 std::chrono::microseconds executionTime) const;
};
// --- src/address.cpp ---
// address.cpp
#include "address.hpp"
#include <sstream>
#include <iomanip>

uint64_t MemoryAddress::getBits(uint8_t start, uint8_t end) const {
    // Extract bits from start to end (inclusive)
    if (start > end) {
        std::swap(start, end);
    }
    
    // Create a mask for the desired bits
    uint64_t mask = 0;
    for (uint8_t i = start; i <= end; ++i) {
        mask |= (1ULL << i);
    }
    
    // Apply the mask and shift to the right
    return (address_ & mask) >> start;
}

uint64_t MemoryAddress::getTag(uint8_t blockOffsetBits, uint8_t indexBits) const {
    // Tag bits are the most significant bits after index and offset
    return address_ >> (blockOffsetBits + indexBits);
}

uint64_t MemoryAddress::getIndex(uint8_t blockOffsetBits, uint8_t indexBits) const {
    // Create a mask for the index bits
    uint64_t mask = (1ULL << indexBits) - 1;
    
    // Apply the mask to the address after shifting out the block offset bits
    return (address_ >> blockOffsetBits) & mask;
}

uint64_t MemoryAddress::getBlockOffset(uint8_t blockOffsetBits) const {
    // Create a mask for the block offset bits
    uint64_t mask = (1ULL << blockOffsetBits) - 1;
    
    // Apply the mask to the address
    return address_ & mask;
}

std::string MemoryAddress::toString() const {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << address_;
    return ss.str();
}
// --- src/cache.cpp ---
// cache.cpp
#include "cache.hpp"
#include <stdexcept>
#include <cassert>

Cache::Cache(const CacheConfig& config) 
    : config_(config), 
      policy_(createPolicy(config.policy)),
      hits_(0),
      misses_(0) {
    
    // Initialize the cache structure
    uint64_t numSets = config_.getNumSets();
    uint64_t numWays = config_.getNumWays();
    
    cacheEntries_.resize(numSets);
    for (auto& set : cacheEntries_) {
        set.resize(numWays);
    }
}

Cache::Cache(const Cache& other)
    : config_(other.config_),
      policy_(other.policy_->clone()),
      cacheEntries_(other.cacheEntries_),
      hits_(other.hits_),
      misses_(other.misses_) {
}

std::pair<uint64_t, uint64_t> Cache::getSetAndTag(const MemoryAddress& address) const {
    uint8_t blockOffsetBits = config_.getBlockOffsetBits();
    uint8_t indexBits = config_.getIndexBits();
    
    uint64_t set = address.getIndex(blockOffsetBits, indexBits);
    uint64_t tag = address.getTag(blockOffsetBits, indexBits);
    
    return {set, tag};
}

std::optional<uint64_t> Cache::findEntry(uint64_t set, uint64_t tag) const {
    // Check all ways in the set
    for (uint64_t way = 0; way < cacheEntries_[set].size(); ++way) {
        const auto& entry = cacheEntries_[set][way];
        if (entry.isValid() && entry.getTag() == tag) {
            return way;
        }
    }
    
    return std::nullopt;
}

CacheResult Cache::allocateEntry(uint64_t set, uint64_t tag, AccessType type) {
    CacheResult result;
    result.hit = false;
    result.latency = config_.accessLatency;
    result.writeBack = false;
    
    // Select a victim entry according to the replacement policy
    uint64_t way = policy_->getVictim(set, cacheEntries_[set].size());
    auto& victimEntry = cacheEntries_[set][way];
    
    // Check if we need to write back the victim
    if (config_.writeBack && victimEntry.isValid() && victimEntry.isDirty()) {
        result.writeBack = true;
        // In a real implementation, we would construct the victim's address and write it back
        // For now, we just set the evicted address to indicate write-back
        
        // Calculate the block offset bits and index bits
        uint8_t blockOffsetBits = config_.getBlockOffsetBits();
        uint8_t indexBits = config_.getIndexBits();
        
        // Reconstruct the address (omitting the block offset bits)
        uint64_t reconstructedAddress = (victimEntry.getTag() << (blockOffsetBits + indexBits)) | 
                                        (set << blockOffsetBits);
        
        result.evictedAddress = MemoryAddress(reconstructedAddress);
    }
    
    // Initialize the new entry
    victimEntry.setValid(true);
    victimEntry.setTag(tag);
    victimEntry.setDirty(type == AccessType::Write && config_.writeBack);
    
    // Update the replacement policy
    policy_->onAccess(set, way);
    
    return result;
}

CacheResult Cache::access(const MemoryAddress& address, AccessType type) {
    CacheResult result;
    result.hit = false;
    result.latency = config_.accessLatency;
    result.writeBack = false;
    
    // Extract set and tag from the address
    auto [set, tag] = getSetAndTag(address);
    
    // Look for the entry in the cache
    auto wayOpt = findEntry(set, tag);
    
    if (wayOpt) {
        // Cache hit
        uint64_t way = *wayOpt;
        hits_++;
        result.hit = true;
        
        // Update the replacement policy
        policy_->onAccess(set, way);
        
        // Handle write hit
        if (type == AccessType::Write) {
            // If write-back, set the dirty bit
            if (config_.writeBack) {
                cacheEntries_[set][way].setDirty(true);
            }
            // If write-through, we would write to memory here
            // But that's handled by the caller
        }
    } else {
        // Cache miss
        misses_++;
        
        // Handle read miss
        if (type == AccessType::Read) {
            // Allocate a new entry
            auto allocResult = allocateEntry(set, tag, type);
            result.latency += allocResult.latency;
            result.writeBack = allocResult.writeBack;
            result.evictedAddress = allocResult.evictedAddress;
        } 
        // Handle write miss
        else if (type == AccessType::Write) {
            // If write-allocate, allocate a new entry
            if (config_.writeAllocate) {
                auto allocResult = allocateEntry(set, tag, type);
                result.latency += allocResult.latency;
                result.writeBack = allocResult.writeBack;
                result.evictedAddress = allocResult.evictedAddress;
            }
            // If no-write-allocate, we just write to memory
            // But that's handled by the caller
        }
    }
    
    return result;
}

void Cache::reset() {
    // Reset all cache entries
    for (auto& set : cacheEntries_) {
        for (auto& entry : set) {
            entry.reset();
        }
    }
    
    // Reset statistics
    hits_ = 0;
    misses_ = 0;
}

// CacheHierarchy implementation
void CacheHierarchy::addCacheLevel(std::unique_ptr<Cache> cache) {
    caches_.push_back(std::move(cache));
}

uint64_t CacheHierarchy::access(const MemoryAddress& address, AccessType type) {
    uint64_t totalLatency = 0;
    bool hit = false;
    
    // Try each cache level in order
    for (size_t i = 0; i < caches_.size(); ++i) {
        auto& cache = caches_[i];
        auto result = cache->access(address, type);
        
        totalLatency += result.latency;
        
        if (result.hit) {
            hit = true;
            break;
        }
        
        // Handle write-back to next level
        if (result.writeBack && result.evictedAddress) {
            // If there's a next level cache, write to it
            if (i + 1 < caches_.size()) {
                auto nextResult = caches_[i + 1]->access(*result.evictedAddress, AccessType::Write);
                totalLatency += nextResult.latency;
            }
            // Otherwise, the write would go to main memory
        }
    }
    
    // If no hit in any cache, memory access would happen here
    // But that's handled by the caller
    
    return totalLatency;
}

void CacheHierarchy::reset() {
    for (auto& cache : caches_) {
        cache->reset();
    }
}

std::vector<std::tuple<double, uint64_t, uint64_t>> CacheHierarchy::getStats() const {
    std::vector<std::tuple<double, uint64_t, uint64_t>> stats;
    
    for (const auto& cache : caches_) {
        stats.emplace_back(
            cache->getHitRate(),
            cache->getHits(),
            cache->getMisses()
        );
    }
    
    return stats;
}
// --- src/cache_policy.cpp ---
// cache_policy.cpp
#include "cache_policy.hpp"
#include <stdexcept>

// LRU Policy Implementation
void LRUPolicy::onAccess(uint64_t set, uint64_t way) {
    // First access to this set
    if (lruList_.find(set) == lruList_.end()) {
        lruList_[set] = std::list<uint64_t>{way};
        lruPositions_[set][way] = lruList_[set].begin();
        return;
    }
    
    auto& list = lruList_[set];
    auto& positions = lruPositions_[set];
    
    // First access to this way in the set
    if (positions.find(way) == positions.end()) {
        list.push_front(way);
        positions[way] = list.begin();
        return;
    }
    
    // Move the accessed way to the front of the list (most recently used)
    list.erase(positions[way]);
    list.push_front(way);
    positions[way] = list.begin();
}

uint64_t LRUPolicy::getVictim(uint64_t set, uint64_t numWays) {
    // If there are not enough entries to fill all ways, return the next empty one
    if (lruList_.find(set) == lruList_.end() || lruList_[set].size() < numWays) {
        // Find the first unused way
        for (uint64_t way = 0; way < numWays; ++way) {
            if (lruPositions_.find(set) == lruPositions_.end() || 
                lruPositions_[set].find(way) == lruPositions_[set].end()) {
                return way;
            }
        }
    }
    
    // Return the least recently used way (back of the list)
    return lruList_[set].back();
}

std::unique_ptr<ReplacementPolicy> LRUPolicy::clone() const {
    return std::make_unique<LRUPolicy>(*this);
}

// FIFO Policy Implementation
void FIFOPolicy::onAccess(uint64_t set, uint64_t way) {
    // First access to this set
    if (fifoQueue_.find(set) == fifoQueue_.end()) {
        fifoQueue_[set] = std::list<uint64_t>{way};
        return;
    }
    
    auto& queue = fifoQueue_[set];
    
    // Only add to queue on first insertion, not on every access
    if (std::find(queue.begin(), queue.end(), way) == queue.end()) {
        queue.push_front(way);
    }
}

uint64_t FIFOPolicy::getVictim(uint64_t set, uint64_t numWays) {
    // If there are not enough entries to fill all ways, return the next empty one
    if (fifoQueue_.find(set) == fifoQueue_.end() || fifoQueue_[set].size() < numWays) {
        // Find the first unused way
        for (uint64_t way = 0; way < numWays; ++way) {
            if (fifoQueue_.find(set) == fifoQueue_.end() || 
                std::find(fifoQueue_[set].begin(), fifoQueue_[set].end(), way) == fifoQueue_[set].end()) {
                return way;
            }
        }
    }
    
    // Return the oldest inserted way (back of the queue)
    return fifoQueue_[set].back();
}

std::unique_ptr<ReplacementPolicy> FIFOPolicy::clone() const {
    return std::make_unique<FIFOPolicy>(*this);
}

// Random Policy Implementation
void RandomPolicy::onAccess(uint64_t set, uint64_t way) {
    // No state to update for random policy
    (void)set;
    (void)way;
}

uint64_t RandomPolicy::getVictim(uint64_t set, uint64_t numWays) {
    // Simply return a random way
    (void)set;
    std::uniform_int_distribution<uint64_t> dist(0, numWays - 1);
    return dist(rng_);
}

std::unique_ptr<ReplacementPolicy> RandomPolicy::clone() const {
    return std::make_unique<RandomPolicy>(*this);
}
// --- src/main.cpp ---
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
// --- src/memory.cpp ---
// memory.cpp
#include "memory.hpp"
#include "cache.hpp"
#include <sstream>
#include <string>
#include <random>
#include <algorithm>
#include <stdexcept>

// FileTraceSource implementation
FileTraceSource::FileTraceSource(const std::string& filename)
    : filename_(filename), lineNumber_(0) {
    file_.open(filename);
    if (!file_.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
}

std::optional<MemoryAccess> FileTraceSource::getNextAccess() {
    std::string line;
    if (std::getline(file_, line)) {
        lineNumber_++;
        
        // Parse the line format (can be customized based on your trace format)
        // Expected format: <address> <R/W>
        std::istringstream iss(line);
        std::string addrStr, typeStr;
        iss >> addrStr >> typeStr;
        
        // Parse address (assuming hexadecimal format)
        uint64_t addr;
        try {
            addr = std::stoull(addrStr, nullptr, 16);
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid address format on line " + 
                                    std::to_string(lineNumber_) + ": " + addrStr);
        }
        
        // Parse access type
        AccessType type;
        if (typeStr == "R" || typeStr == "r") {
            type = AccessType::Read;
        } else if (typeStr == "W" || typeStr == "w") {
            type = AccessType::Write;
        } else {
            throw std::runtime_error("Invalid access type on line " + 
                                    std::to_string(lineNumber_) + ": " + typeStr);
        }
        
        return MemoryAccess(MemoryAddress(addr), type);
    }
    
    return std::nullopt;
}

void FileTraceSource::reset() {
    file_.clear();
    file_.seekg(0);
    lineNumber_ = 0;
}

std::unique_ptr<MemoryTraceSource> FileTraceSource::clone() const {
    return std::make_unique<FileTraceSource>(filename_);
}

// SyntheticTraceSource implementation
SyntheticTraceSource::SyntheticTraceSource(Pattern pattern, 
                                          uint64_t startAddress, 
                                          uint64_t endAddress,
                                          uint64_t numAccesses,
                                          double readRatio)
    : pattern_(pattern), 
      startAddress_(startAddress), 
      endAddress_(endAddress),
      numAccesses_(numAccesses),
      readRatio_(readRatio),
      currentAccess_(0),
      rng_(std::random_device{}()) {
    
    // For looping pattern, pre-generate a set of addresses to loop through
    if (pattern_ == Pattern::Looping) {
        const uint64_t loopSize = std::min<uint64_t>(100, endAddress_ - startAddress_);
        loopAddresses_.reserve(loopSize);
        
        // Generate random addresses for the loop
        std::uniform_int_distribution<uint64_t> dist(startAddress_, endAddress_);
        for (uint64_t i = 0; i < loopSize; ++i) {
            loopAddresses_.push_back(dist(rng_));
        }
    }
}

std::optional<MemoryAccess> SyntheticTraceSource::getNextAccess() {
    if (currentAccess_ >= numAccesses_) {
        return std::nullopt;
    }
    
    uint64_t addr = generateAddress();
    AccessType type = generateAccessType();
    
    currentAccess_++;
    
    return MemoryAccess(MemoryAddress(addr), type);
}

void SyntheticTraceSource::reset() {
    currentAccess_ = 0;
}

std::unique_ptr<MemoryTraceSource> SyntheticTraceSource::clone() const {
    return std::make_unique<SyntheticTraceSource>(*this);
}

uint64_t SyntheticTraceSource::generateAddress() {
    switch (pattern_) {
        case Pattern::Sequential: {
            // Sequential access pattern
            return startAddress_ + (currentAccess_ % (endAddress_ - startAddress_));
        }
        
        case Pattern::Random: {
            // Random access pattern
            std::uniform_int_distribution<uint64_t> dist(startAddress_, endAddress_ - 1);
            return dist(rng_);
        }
        
        case Pattern::Strided: {
            // Strided access pattern (e.g., for matrix traversal)
            const uint64_t stride = 64; // Example: 64 bytes stride
            return startAddress_ + ((currentAccess_ * stride) % (endAddress_ - startAddress_));
        }
        
        case Pattern::Looping: {
            // Looping through a small set of addresses
            return loopAddresses_[currentAccess_ % loopAddresses_.size()];
        }
        
        default:
            throw std::runtime_error("Unknown pattern type");
    }
}

AccessType SyntheticTraceSource::generateAccessType() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return (dist(rng_) < readRatio_) ? AccessType::Read : AccessType::Write;
}

// FunctionTraceSource implementation
FunctionTraceSource::FunctionTraceSource(GeneratorFunction generator, ResetFunction reset)
    : generator_(std::move(generator)), reset_(std::move(reset)), clonable_(false) {
}

std::optional<MemoryAccess> FunctionTraceSource::getNextAccess() {
    return generator_();
}

void FunctionTraceSource::reset() {
    reset_();
}

std::unique_ptr<MemoryTraceSource> FunctionTraceSource::clone() const {
    if (!clonable_) {
        throw std::runtime_error("This FunctionTraceSource is not clonable");
    }
    
    return std::make_unique<FunctionTraceSource>(*this);
}

// MainMemory implementation
MainMemory::MainMemory(uint64_t accessLatency)
    : accessLatency_(accessLatency), reads_(0), writes_(0) {
}

uint64_t MainMemory::access(const MemoryAddress& address, AccessType type) {
    // Update statistics
    if (type == AccessType::Read) {
        reads_++;
    } else {
        writes_++;
    }
    
    // Return access latency
    return accessLatency_;
}

void MainMemory::reset() {
    reads_ = 0;
    writes_ = 0;
}
// --- src/metrics.cpp ---
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
// --- tests/cache_test.cpp ---
// cache_test.cpp
#include "address.hpp"
#include "cache.hpp"
#include "cache_policy.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <string>

// Define a simple test framework
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

// Test memory address functionality
void testMemoryAddress() {
    TEST_CASE("MemoryAddress Construction and Bit Extraction") {
        MemoryAddress addr(0x12345678ABCDEF);
        
        // Test address storage
        ASSERT(addr.getAddress() == 0x12345678ABCDEF);
        
        // Test bit extraction
        ASSERT(addr.getBits(0, 7) == 0xEF);
        ASSERT(addr.getBits(8, 15) == 0xCD);
        ASSERT(addr.getBits(16, 23) == 0xAB);
        
        // Test tag/index/offset extraction for a specific cache configuration
        // For a cache with 64B blocks (6 bits offset) and 64 sets (6 bits index)
        uint8_t blockOffsetBits = 6;
        uint8_t indexBits = 6;
        
        // Expected values:
        // Block offset: bits 0-5 (0x2F = 0b101111)
        // Index: bits 6-11 (0x13 = 0b010011)
        // Tag: bits 12+ (0x12345678AB = 0b1001000110100010101100111000101010101011)
        MemoryAddress addr2(0x12345678ABCDEF);
        ASSERT(addr2.getBlockOffset(blockOffsetBits) == 0x2F);
        ASSERT(addr2.getIndex(blockOffsetBits, indexBits) == 0x13);
        ASSERT(addr2.getTag(blockOffsetBits, indexBits) == 0x12345678AB);
    }
    END_TEST;
}

// Test cache configuration
void testCacheConfig() {
    TEST_CASE("CacheConfig Calculations") {
        // Direct-mapped cache
        CacheConfig dmConfig;
        dmConfig.organization = CacheConfig::Organization::DirectMapped;
        dmConfig.size = 32 * 1024; // 32KB
        dmConfig.blockSize = 64;   // 64B blocks
        dmConfig.associativity = 1; // Direct-mapped
        
        // Should have 32KB/64B = 512 sets
        ASSERT(dmConfig.getNumSets() == 512);
        ASSERT(dmConfig.getNumWays() == 1);
        ASSERT(dmConfig.getBlockOffsetBits() == 6); // 2^6 = 64
        ASSERT(dmConfig.getIndexBits() == 9);      // 2^9 = 512
        
        // Set-associative cache
        CacheConfig saConfig;
        saConfig.organization = CacheConfig::Organization::SetAssociative;
        saConfig.size = 32 * 1024; // 32KB
        saConfig.blockSize = 64;   // 64B blocks
        saConfig.associativity = 4; // 4-way set-associative
        
        // Should have 32KB/(64B*4) = 128 sets
        ASSERT(saConfig.getNumSets() == 128);
        ASSERT(saConfig.getNumWays() == 4);
        ASSERT(saConfig.getBlockOffsetBits() == 6); // 2^6 = 64
        ASSERT(saConfig.getIndexBits() == 7);      // 2^7 = 128
        
        // Fully associative cache
        CacheConfig faConfig;
        faConfig.organization = CacheConfig::Organization::FullyAssociative;
        faConfig.size = 32 * 1024; // 32KB
        faConfig.blockSize = 64;   // 64B blocks
        
        // Should have 1 set and 32KB/64B = 512 ways
        ASSERT(faConfig.getNumSets() == 1);
        ASSERT(faConfig.getNumWays() == 512);
        ASSERT(faConfig.getBlockOffsetBits() == 6); // 2^6 = 64
        ASSERT(faConfig.getIndexBits() == 0);      // No index bits for fully associative
    }
    END_TEST;
}

// Test LRU replacement policy
void testLRUPolicy() {
    TEST_CASE("LRU Replacement Policy") {
        LRUPolicy policy;
        
        // Access pattern: 0, 1, 2, 0, 3
        // LRU order after these accesses should be: 1, 2, 0, 3 (oldest to newest)
        policy.onAccess(0, 0); // Set 0, Way 0
        policy.onAccess(0, 1); // Set 0, Way 1
        policy.onAccess(0, 2); // Set 0, Way 2
        policy.onAccess(0, 0); // Set 0, Way 0 again
        policy.onAccess(0, 3); // Set 0, Way 3
        
        // With 4 ways, we should replace way 1 (least recently used)
        ASSERT(policy.getVictim(0, 4) == 1);
        
        // Access way 1, so way 2 becomes LRU
        policy.onAccess(0, 1);
        ASSERT(policy.getVictim(0, 4) == 2);
        
        // Test a different set
        policy.onAccess(1, 0); // Set 1, Way 0
        policy.onAccess(1, 1); // Set 1, Way 1
        
        // With 4 ways, but only 2 used, it should return an unused way
        ASSERT(policy.getVictim(1, 4) == 2 || policy.getVictim(1, 4) == 3);
        
        // Fill all ways in set 1
        policy.onAccess(1, 2); // Set 1, Way 2
        policy.onAccess(1, 3); // Set 1, Way 3
        
        // LRU is way 0
        ASSERT(policy.getVictim(1, 4) == 0);
    }
    END_TEST;
}

// Test FIFO replacement policy
void testFIFOPolicy() {
    TEST_CASE("FIFO Replacement Policy") {
        FIFOPolicy policy;
        
        // Access pattern: 0, 1, 2, 0, 3
        // FIFO order should still be: 0, 1, 2, 3 (oldest to newest)
        // Re-accessing 0 doesn't change its position
        policy.onAccess(0, 0); // Set 0, Way 0
        policy.onAccess(0, 1); // Set 0, Way 1
        policy.onAccess(0, 2); // Set 0, Way 2
        policy.onAccess(0, 0); // Set 0, Way 0 again - doesn't affect FIFO order
        policy.onAccess(0, 3); // Set 0, Way 3
        
        // With 4 ways, we should replace way 0 (first in)
        ASSERT(policy.getVictim(0, 4) == 0);
        
        // Even after accessing way 0 again, it should still be the victim
        policy.onAccess(0, 0);
        ASSERT(policy.getVictim(0, 4) == 0);
        
        // If we evict way 0 and insert a new way (4), then way 1 becomes the oldest
        policy.onAccess(0, 4);
        ASSERT(policy.getVictim(0, 5) == 1);
    }
    END_TEST;
}

// Test Random replacement policy
void testRandomPolicy() {
    TEST_CASE("Random Replacement Policy") {
        RandomPolicy policy;
        
        // Just check that it returns a valid way
        for (int i = 0; i < 100; ++i) {
            uint64_t victim = policy.getVictim(0, 4);
            ASSERT(victim < 4);
        }
    }
    END_TEST;
}

// Test direct-mapped cache
void testDirectMappedCache() {
    TEST_CASE("Direct-Mapped Cache") {
        // Create a tiny direct-mapped cache for testing
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.associativity = 1; // Direct-mapped
        config.policy = "LRU";    // Irrelevant for direct-mapped
        
        Cache cache(config);
        
        // This cache has 4 sets (256/64)
        // Addresses will map as follows:
        // Block offset: bits 0-5
        // Index: bits 6-7 (2 bits for 4 sets)
        // Tag: bits 8+
        
        // First access - miss
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // Same address - hit
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        
        // Different address, same set - miss and should evict the first block
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        
        // Now the first address should miss
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // Different address, different set - miss but shouldn't evict the previous block
        result = cache.access(MemoryAddress(0x40), AccessType::Read);
        ASSERT(!result.hit);
        
        // Previous address in the second set should still be cached
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(result.hit);
        
        // Check stats
        ASSERT(cache.getHits() == 2);
        ASSERT(cache.getMisses() == 4);
        ASSERT(cache.getHitRate() == 2.0 / 6.0);
    }
    END_TEST;
}

// Test set-associative cache
void testSetAssociativeCache() {
    TEST_CASE("Set-Associative Cache with LRU") {
        // Create a tiny 2-way set-associative cache
        CacheConfig config;
        config.organization = CacheConfig::Organization::SetAssociative;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.associativity = 2; // 2-way set-associative
        config.policy = "LRU";
        
        Cache cache(config);
        
        // This cache has 2 sets (256/(64*2))
        // Addresses will map as follows:
        // Block offset: bits 0-5
        // Index: bit 6 (1 bit for 2 sets)
        // Tag: bits 7+
        
        // First access to set 0 - miss
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // Second access to set 0 - miss, but doesn't evict the first block due to associativity
        result = cache.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(!result.hit);
        
        // Both addresses should now hit
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        
        result = cache.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(result.hit);
        
        // Third access to set 0 - miss and should evict the least recently used block (0x0)
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        
        // 0x0 should now miss
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        
        // But 0x80 should still hit
        result = cache.access(MemoryAddress(0x80), AccessType::Read);
        ASSERT(result.hit);
        
        // Check stats
        ASSERT(cache.getHits() == 3);
        ASSERT(cache.getMisses() == 4);
    }
    END_TEST;
}

// Test write policies
void testWritePolicies() {
    TEST_CASE("Write-Back Policy") {
        // Create a write-back cache
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.writeBack = true;
        config.writeAllocate = true;
        
        Cache cache(config);
        
        // Read miss - block is loaded
        auto result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
        ASSERT(!result.writeBack);
        
        // Write hit - block is marked dirty but no write-back
        result = cache.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(result.hit);
        ASSERT(!result.writeBack);
        
        // Evict the dirty block - should trigger write-back
        result = cache.access(MemoryAddress(0x100), AccessType::Read);
        ASSERT(!result.hit);
        ASSERT(result.writeBack);
        ASSERT(result.evictedAddress.has_value());
        ASSERT(result.evictedAddress->getAddress() == 0x0);
    }
    END_TEST;
    
    TEST_CASE("Write-Allocate Policy") {
        // Create a write-allocate cache
        CacheConfig config;
        config.organization = CacheConfig::Organization::DirectMapped;
        config.size = 256;       // 256B total
        config.blockSize = 64;   // 64B blocks
        config.writeBack = true;
        config.writeAllocate = true;
        
        Cache cache(config);
        
        // Write miss - block is allocated
        auto result = cache.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(!result.hit);
        
        // Read hit - the block was allocated on the write miss
        result = cache.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(result.hit);
        
        // Create a no-write-allocate cache
        config.writeAllocate = false;
        Cache cache2(config);
        
        // Write miss - block is not allocated (write goes directly to memory)
        result = cache2.access(MemoryAddress(0x0), AccessType::Write);
        ASSERT(!result.hit);
        
        // Read miss - the block was not allocated on the write miss
        result = cache2.access(MemoryAddress(0x0), AccessType::Read);
        ASSERT(!result.hit);
    }
    END_TEST;
}

// Run all tests
int main() {
    std::cout << "Running cache simulator tests..." << std::endl;
    
    testMemoryAddress();
    testCacheConfig();
    testLRUPolicy();
    testFIFOPolicy();
    testRandomPolicy();
    testDirectMappedCache();
    testSetAssociativeCache();
    testWritePolicies();
    
    std::cout << "All tests completed." << std::endl;
    return 0;
}
