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
