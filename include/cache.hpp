// cache.hpp
#pragma once

#include "address.hpp"
#include "cache_policy.hpp"
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <iostream>

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

    // Copy constructor and assignment operator for entry transfers
    CacheEntry(const CacheEntry& other) = default;
    CacheEntry& operator=(const CacheEntry& other) = default;

private:
    bool valid_;   // Is the entry valid
    bool dirty_;   // Has the entry been modified (for write-back caches)
    uint64_t tag_; // Tag bits from the address
};

enum class InclusionPolicy {
    Inclusive,    // Lower level cache contains all blocks from upper level
    Exclusive,    // Lower level cache contains no blocks from upper level
    NINE          // Non-Inclusive Non-Exclusive
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
    uint64_t level;         // Cache level (1 for L1, 2 for L2, etc.)
    InclusionPolicy inclusionPolicy = InclusionPolicy::Inclusive; // Default to inclusive

    // Constructor with default values
    CacheConfig() :
        organization(Organization::SetAssociative),
        size(65536),        // 64KB default
        blockSize(64),      // 64B blocks
        associativity(8),   // 8-way set associative
        policy("LRU"),
        accessLatency(1),   // 1 cycle latency
        writeBack(true),    // Default to write-back
        writeAllocate(true), // Default to write-allocate
        level(1),           // Default to L1
        inclusionPolicy(InclusionPolicy::Inclusive) // Default to inclusive
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
    std::optional<CacheEntry> evictedEntry;
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

    // Get the cache level
    [[nodiscard]] uint64_t getLevel() const { return config_.level; }

    // Reconstruct full address from set and tag
    [[nodiscard]] MemoryAddress reconstructAddress(uint64_t set, uint64_t tag) const;

    [[nodiscard]] std::pair<uint64_t, uint64_t> getSetAndTag(const MemoryAddress& address) const;
    
    // Reset the cache and statistics
    void reset();
    
    // Get configuration
    [[nodiscard]] const CacheConfig& getConfig() const { return config_; }
    
    
    void dumpSetState(uint64_t set) const {
        std::cout << "CACHE STATE - Set " << set << ":" << std::endl;
        for (size_t way = 0; way < cacheEntries_[set].size(); ++way) {
            const auto& entry = cacheEntries_[set][way];
            std::cout << "  Way " << way << ": valid=" << entry.isValid() 
                      << ", dirty=" << entry.isDirty()
                      << ", tag=0x" << std::hex << entry.getTag() 
                      << std::dec << std::endl;
        }
    }    

    bool contains(const MemoryAddress& address) const {
        auto [set, tag] = getSetAndTag(address);
        return findEntry(set, tag).has_value();
    }

    // Get the complete cache entry (for exclusive cache transfers)
    [[nodiscard]] std::optional<CacheEntry> getEntry(const MemoryAddress& address) const;

    // Remove an entry from the cache (for exclusive cache transfers)
    void invalidateEntry(const MemoryAddress& address);

    // Force an entry into the cache without normal replacement (for promotions)
    CacheResult forceEntry(const MemoryAddress& address, const CacheEntry& entry, AccessType type);



private:
    CacheConfig config_;
    std::unique_ptr<ReplacementPolicy> policy_;
    
    // Cache structure: vector of sets, each set is a vector of ways (entries)
    std::vector<std::vector<CacheEntry>> cacheEntries_;
    
    // Performance statistics
    uint64_t hits_;
    uint64_t misses_;
    
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
    [[nodiscard]] std::pair<uint64_t, bool> access(const MemoryAddress& address, AccessType type);

    // Reset all caches in the hierarchy
    void reset();
    
    // Get statistics for all cache levels
    [[nodiscard]] std::vector<std::tuple<double, uint64_t, uint64_t>> getStats() const;

    // Get a reference to a specific cache level
    [[nodiscard]] const Cache& getCacheLevel(size_t level) const {
        if (level >= caches_.size()) {
            throw std::out_of_range("Cache level out of range");
        }
        return *caches_[level];
    }

    // Get a non-const reference to a specific cache level
    [[nodiscard]] Cache& getCacheLevel(size_t level) {
        if (level >= caches_.size()) {
            throw std::out_of_range("Cache level out of range");
        }
        return *caches_[level];
    }

    // Get the number of cache levels
    [[nodiscard]] size_t getNumLevels() const {
        return caches_.size();
    }

private:
    std::vector<std::unique_ptr<Cache>> caches_;

    // Helper method for backinvalidation
    void backinvalidate(MemoryAddress address, size_t fromLevel);

    // Explicit tracking of the most recently evicted block from L1
    struct EvictionTracker {
        bool hasEviction = false;
        MemoryAddress address = MemoryAddress(0);
        CacheEntry entry;
    };
    EvictionTracker evictionTracker_;
};
