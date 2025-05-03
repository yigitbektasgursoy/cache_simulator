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

// Fixed FIFO Policy Implementation
void FIFOPolicy::onAccess(uint64_t set, uint64_t way) {
    // First access to this set
    if (fifoQueue_.find(set) == fifoQueue_.end()) {
        fifoQueue_[set] = std::list<uint64_t>{way};
        return;
    }

    auto& queue = fifoQueue_[set];

    // Only add to queue on first insertion, not on every access
    if (std::find(queue.begin(), queue.end(), way) == queue.end()) {
        queue.push_back(way);  // FIXED: Changed from push_front to push_back
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

    // Return the oldest inserted way (front of the queue)
    return fifoQueue_[set].front();  // FIXED: Changed from back() to front()
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
