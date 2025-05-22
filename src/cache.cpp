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

// In cache.cpp - ensure findEntry is working correctly
std::optional<uint64_t> Cache::findEntry(uint64_t set, uint64_t tag) const {
    // Check all ways in the set
    for (uint64_t way = 0; way < cacheEntries_[set].size(); ++way) {
        const auto& entry = cacheEntries_[set][way];
        if (entry.isValid() && entry.getTag() == tag) {
            //std::cout << "DEBUG: Found at way " << way << "\n";
            return way;
        }
    }
    
    //std::cout << "DEBUG: Not found in cache\n";
    return std::nullopt;
}

CacheResult Cache::allocateEntry(uint64_t set, uint64_t tag, AccessType type) {
    CacheResult result;
    result.hit = false;
    result.latency = 0;  // Don't double-count latency
    result.writeBack = false;
    
    // Select a victim entry according to the replacement policy
    uint64_t way = policy_->getVictim(set, cacheEntries_[set].size());
    
    auto& victimEntry = cacheEntries_[set][way];
    
    // Store the victim entry before we overwrite it
    if (victimEntry.isValid()) {
        // Save the entire victim entry
        result.evictedEntry = victimEntry;  // Store a copy

        // Also check if it needs writeback
        if (config_.writeBack && victimEntry.isDirty()) {
            result.writeBack = true;

            // Reconstruct the address
            uint8_t blockOffsetBits = config_.getBlockOffsetBits();
            uint8_t indexBits = config_.getIndexBits();

            uint64_t reconstructedAddress = (victimEntry.getTag() << (blockOffsetBits + indexBits)) |
                                           (set << blockOffsetBits);

            result.evictedAddress = MemoryAddress(reconstructedAddress);
        } else {
            // Even if not dirty, we still want to track the evicted address
            uint8_t blockOffsetBits = config_.getBlockOffsetBits();
            uint8_t indexBits = config_.getIndexBits();

            uint64_t reconstructedAddress = (victimEntry.getTag() << (blockOffsetBits + indexBits)) |
                                           (set << blockOffsetBits);

            result.evictedAddress = MemoryAddress(reconstructedAddress);
        }
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
        }
    } else {
        // Cache miss
        misses_++;

        // Handle the miss
        if (type == AccessType::Read ||
            (type == AccessType::Write && config_.writeAllocate)) {
            // Allocate a new entry
            auto allocResult = allocateEntry(set, tag, type);
            result.latency += allocResult.latency;
            result.writeBack = allocResult.writeBack;
            result.evictedAddress = allocResult.evictedAddress;

            // IMPORTANT: After allocation, search for the entry again
            // to ensure it's properly recognized as present in the cache
            auto checkWayOpt = findEntry(set, tag);
            if (!checkWayOpt) {
                //std::cout << "WARNING: Entry not found after allocation!\n";
            }
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
    // Reset the replacement policy's internal state
    policy_->reset();
    
    // Reset statistics
    hits_ = 0;
    misses_ = 0;
}

// CacheHierarchy implementation
void CacheHierarchy::addCacheLevel(std::unique_ptr<Cache> cache) {
    caches_.push_back(std::move(cache));
}

// Rewrite the CacheHierarchy::access method with an explicit victim caching approach
std::pair<uint64_t, bool> CacheHierarchy::access(const MemoryAddress& address, AccessType type) {
    uint64_t totalLatency = 0;
    bool hitInCache = false;

    // Reset eviction tracker at the start of each operation
    evictionTracker_.hasEviction = false;

    // Determine if we have an exclusive L2 cache
    bool hasExclusiveL2 = (caches_.size() > 1 &&
                          caches_[1]->getConfig().inclusionPolicy == InclusionPolicy::Exclusive);

    // For exclusive caches, we need to track what's in L1 before access
    bool wasInL1 = false;
    CacheEntry l1EntryBeforeAccess;

    if (hasExclusiveL2) {
        auto& l1Cache = *caches_[0];
        auto entryOpt = l1Cache.getEntry(address);
        wasInL1 = entryOpt.has_value();
        if (wasInL1) {
            l1EntryBeforeAccess = *entryOpt;
        }
    }

    // First try L1 cache
    auto& l1Cache = *caches_[0];
    auto l1Result = l1Cache.access(address, type);
    totalLatency += l1Result.latency;

    // SPECIAL HANDLING: Explicitly check for evictions from L1
    if (hasExclusiveL2 && l1Result.evictedAddress) {
        // Get the address of the evicted block
        MemoryAddress evictedAddr = *l1Result.evictedAddress;

        // Store this eviction so we can handle it at the end
        evictionTracker_.hasEviction = true;
        evictionTracker_.address = evictedAddr;

        // We need the content of the evicted entry, but it's already gone from L1
        // If we captured it earlier, use that, otherwise use what's in the result
        if (l1Result.evictedEntry) {
            evictionTracker_.entry = *l1Result.evictedEntry;
        } else {
            // Create a basic valid entry with the correct tag
            auto [set, tag] = l1Cache.getSetAndTag(evictedAddr);
            evictionTracker_.entry = CacheEntry();
            evictionTracker_.entry.setValid(true);
            evictionTracker_.entry.setTag(tag);
        }
    }

    if (l1Result.hit) {
        // L1 hit - no need to check other levels
        hitInCache = true;
        return {totalLatency, hitInCache};
    }

    // L1 miss - try other cache levels
    for (size_t i = 1; i < caches_.size(); ++i) {
        auto& cache = caches_[i];
        auto& config = cache->getConfig();

        auto result = cache->access(address, type);
        totalLatency += result.latency;

        if (result.hit) {
            hitInCache = true;

            // For exclusive caches, transfer the block from lower level to L1
            if (config.inclusionPolicy == InclusionPolicy::Exclusive) {
                // Get the entry from this cache level
                auto entry = cache->getEntry(address);

                if (entry) {
                    // Remove from this level
                    cache->invalidateEntry(address);

                    // Add to L1 (may cause an eviction from L1)
                    auto l1EvictResult = l1Cache.forceEntry(address, *entry, type);

                    // Handle L1 eviction (victim caching)
                    if (l1EvictResult.evictedAddress) {
                        MemoryAddress evictedAddr = *l1EvictResult.evictedAddress;

                        // Capture the eviction data
                        evictionTracker_.hasEviction = true;
                        evictionTracker_.address = evictedAddr;

                        if (l1EvictResult.evictedEntry) {
                            evictionTracker_.entry = *l1EvictResult.evictedEntry;
                        } else {
                            // Create a basic valid entry with the correct tag
                            auto [set, tag] = l1Cache.getSetAndTag(evictedAddr);
                            evictionTracker_.entry = CacheEntry();
                            evictionTracker_.entry.setValid(true);
                            evictionTracker_.entry.setTag(tag);
                        }
                    }
                }
            }

            // Found in a cache level, no need to check further
            break;
        }
    }

    // Miss in all cache levels - need to fetch from memory and allocate
    if (!hitInCache && (type == AccessType::Read ||
                      (type == AccessType::Write && l1Cache.getConfig().writeAllocate))) {

        // Handle allocation based on cache policies
        if (hasExclusiveL2) {
            // For exclusive caches, ensure the block is only in L1
            // The L1.access() call above already allocated the block in L1

            // Just ensure the block isn't also in L2 (cleanup)
            if (caches_.size() > 1) {
                caches_[1]->invalidateEntry(address);
            }
        } else {
            // For inclusive caches, ensure the block is in all levels
            for (size_t i = 1; i < caches_.size(); ++i) {
                auto& cache = caches_[i];

                // Only allocate for inclusive policy
                if (cache->getConfig().inclusionPolicy == InclusionPolicy::Inclusive) {
                    auto result = cache->access(address, type);
                    totalLatency += result.latency;

                    // Handle backinvalidation if needed
                    if (result.evictedAddress) {
                        backinvalidate(*result.evictedAddress, i);
                    }
                }
            }
        }
    }

    // CRITICAL FIX: Process any L1 evictions for exclusive caches
    if (hasExclusiveL2 && evictionTracker_.hasEviction) {
        auto& l2Cache = *caches_[1];

        // Check that the evicted block isn't the one we just accessed
        if (evictionTracker_.address != address) {
            // Move the evicted block to L2 (victim caching)
            l2Cache.forceEntry(evictionTracker_.address, evictionTracker_.entry, AccessType::Write);
        }
    }

    return {totalLatency, hitInCache};
}

// Helper method for backinvalidation
void CacheHierarchy::backinvalidate(MemoryAddress address, size_t fromLevel) {
    // Backinvalidate in all levels above the specified level
    for (size_t i = 0; i < fromLevel; ++i) {
        caches_[i]->invalidateEntry(address);
    }
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

std::optional<CacheEntry> Cache::getEntry(const MemoryAddress& address) const {
    auto [set, tag] = getSetAndTag(address);
    auto wayOpt = findEntry(set, tag);

    if (wayOpt) {
        uint64_t way = *wayOpt;
        return cacheEntries_[set][way];
    }

    return std::nullopt;
}

void Cache::invalidateEntry(const MemoryAddress& address) {
    auto [set, tag] = getSetAndTag(address);
    auto wayOpt = findEntry(set, tag);

    if (wayOpt) {
        uint64_t way = *wayOpt;
        cacheEntries_[set][way].reset();
    }
}

// In src/cache.cpp
CacheResult Cache::forceEntry(const MemoryAddress& address, const CacheEntry& entry, AccessType type) {
    CacheResult result;
    result.hit = false;
    result.latency = config_.accessLatency;
    result.writeBack = false;

    auto [set, tag] = getSetAndTag(address);
    auto wayOpt = findEntry(set, tag);

    uint64_t way;
    if (wayOpt) {
        // Entry already exists - replace it
        way = *wayOpt;
    } else {
        // Need to allocate a new entry - may cause eviction
        way = policy_->getVictim(set, cacheEntries_[set].size());

        // Save the victim entry before overwriting (NEW CODE)
        auto& victimEntry = cacheEntries_[set][way];
        if (victimEntry.isValid()) {
            result.evictedEntry = victimEntry;
        }

        // Check if we're evicting a valid, dirty entry
        if (victimEntry.isValid() && victimEntry.isDirty()) {
            result.writeBack = true;

            // Calculate the evicted address
            uint64_t victimTag = victimEntry.getTag();
            MemoryAddress evictedAddr = reconstructAddress(set, victimTag);
            result.evictedAddress = evictedAddr;
        }
    }

    // Copy the entry
    cacheEntries_[set][way] = entry;
    cacheEntries_[set][way].setTag(tag);
    cacheEntries_[set][way].setValid(true);

    // Update dirty bit if this is a write
    if (type == AccessType::Write && config_.writeBack) {
        cacheEntries_[set][way].setDirty(true);
    }

    // Update the replacement policy
    policy_->onAccess(set, way);

    return result;
}

MemoryAddress Cache::reconstructAddress(uint64_t set, uint64_t tag) const {
    uint8_t blockOffsetBits = config_.getBlockOffsetBits();
    uint8_t indexBits = config_.getIndexBits();

    // Reconstruct the full address
    uint64_t address = (tag << (blockOffsetBits + indexBits)) | (set << blockOffsetBits);
    return MemoryAddress(address);
}