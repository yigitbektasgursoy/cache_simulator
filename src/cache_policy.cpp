// cache_policy.cpp
#include "cache_policy.hpp"
#include <set>
#include <random>

// LRU Policy Implementation
void LRUPolicy::onAccess(uint64_t set, uint64_t way) {

    // If this is the first access to this set, create a new list
    if (lruList_.find(set) == lruList_.end()) {
        lruList_[set] = std::list<uint64_t>{way};
        lruPositions_[set][way] = lruList_[set].begin();
        
        return;
    }
    
    auto& list = lruList_[set];
    auto& positions = lruPositions_[set];
    
    // If this way is already in the list, remove it
    if (positions.find(way) != positions.end()) {
        list.erase(positions[way]);
    }
    
    // Add the way to the front (MRU position)
    list.push_front(way);
    positions[way] = list.begin();

}

uint64_t LRUPolicy::getVictim(uint64_t set, uint64_t numWays) {

    // If this set doesn't exist yet, return way 0
    if (lruList_.find(set) == lruList_.end()) {
        return 0;
    }
    
    auto& list = lruList_[set];

    
    // If the set isn't full yet, find an unused way
    if (list.size() < numWays) {
        // Find the first unused way
        std::set<uint64_t> usedWays;
        for (uint64_t way : list) {
            usedWays.insert(way);
        }
        
        for (uint64_t way = 0; way < numWays; ++way) {
            if (usedWays.find(way) == usedWays.end()) {
                return way;
            }
        }
    }
    
    // Return the least recently used way (back of the list)
    uint64_t victim = list.back();
    return victim;
}


std::unique_ptr<ReplacementPolicy> LRUPolicy::clone() const {
    return std::make_unique<LRUPolicy>(*this);
}


// Implementation of FIFOPolicy methods
void FIFOPolicy::onAccess(uint64_t set, uint64_t way) {

    // Initialize counter if needed
    if (circularCounters_.find(set) == circularCounters_.end()) {
        circularCounters_[set] = 0;
    }
    
    auto& used = usedWays_[set];

    // Only track ways if we haven't seen them before
    if (used.find(way) == used.end()) {
        used.insert(way);
    }

}

uint64_t FIFOPolicy::getVictim(uint64_t set, uint64_t numWays) {

    // Initialize counter if needed
    if (circularCounters_.find(set) == circularCounters_.end()) {
        circularCounters_[set] = 0;
    }
    
    auto& used = usedWays_[set];

    
    // Initial filling phase - find first unused way
    if (used.size() < numWays) {
        for (uint64_t way = 0; way < numWays; ++way) {
            if (used.find(way) == used.end()) {
                return way;
            }
        }
    }
    
    // Get current victim from counter
    uint64_t victim = circularCounters_[set];
    
    // Update counter for next time (circular)
    circularCounters_[set] = (circularCounters_[set] + 1) % numWays;

    
    return victim;
}

std::unique_ptr<ReplacementPolicy> FIFOPolicy::clone() const {
    return std::make_unique<FIFOPolicy>(*this);
}


// Updated RandomPolicy onAccess and getVictim methods
void RandomPolicy::onAccess(uint64_t set, uint64_t way) {
    // When a way is accessed, mark it as used
    if (usedWays_.find(set) == usedWays_.end()) {
        // Initialize with enough space for the ways in this set
        usedWays_[set] = std::vector<bool>(32, false); // 32 is a safe maximum
    }
    
    // Mark this way as used
    if (way < usedWays_[set].size()) {
        usedWays_[set][way] = true;
    }
}

uint64_t RandomPolicy::getVictim(uint64_t set, uint64_t numWays) {
    // Create a fresh RNG for each call
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<uint64_t> dist(0, numWays - 1);
    
    // If there's no record for this set yet, initialize it
    if (usedWays_.find(set) == usedWays_.end()) {
        usedWays_[set] = std::vector<bool>(numWays, false);
    }
    
    // Ensure the vector is large enough
    if (usedWays_[set].size() < numWays) {
        usedWays_[set].resize(numWays, false);
    }
    
    // First check if any ways are unused
    for (uint64_t way = 0; way < numWays; ++way) {
        if (!usedWays_[set][way]) {

            // Mark it as used now
            usedWays_[set][way] = true;
            return way;
        }
    }
    
    // All ways are used, select a random victim
    uint64_t victim = dist(rng);
    return victim;
}

std::unique_ptr<ReplacementPolicy> RandomPolicy::clone() const {
    return std::make_unique<RandomPolicy>(*this);
}


// LRU Policy reset implementation
void LRUPolicy::reset() {
    // Clear LRU tracking data structures
    lruList_.clear();
    lruPositions_.clear();
}

void FIFOPolicy::reset() {
    // Clear all data structures
    circularCounters_.clear();
    usedWays_.clear();
}

// Random Policy reset implementation
void RandomPolicy::reset() {
    // Clear the map tracking used ways
    usedWays_.clear();
}