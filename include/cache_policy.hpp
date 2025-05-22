// cache_policy.hpp
#pragma once

#include <memory>
#include <vector>
#include <list>
#include <unordered_map>
#include <random>
#include <functional>
#include <set>

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
    
    // Reset the policy's internal state
    virtual void reset() = 0;
};

// LRU (Least Recently Used) policy
class LRUPolicy : public ReplacementPolicy {
public:
    LRUPolicy() = default;
    
    void onAccess(uint64_t set, uint64_t way) override;
    [[nodiscard]] uint64_t getVictim(uint64_t set, uint64_t numWays) override;
    [[nodiscard]] std::unique_ptr<ReplacementPolicy> clone() const override;
    void reset() override; // Add this line

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
    void reset() override;

private:
    // Simple counter for each set - points to next way to evict
    std::unordered_map<uint64_t, uint64_t> circularCounters_;
    
    // Track which ways have been seen for each set
    std::unordered_map<uint64_t, std::set<uint64_t>> usedWays_;
};

// Random replacement policy
class RandomPolicy : public ReplacementPolicy {
public:
    RandomPolicy() = default;
    
    void onAccess(uint64_t set, uint64_t way) override;
    [[nodiscard]] uint64_t getVictim(uint64_t set, uint64_t numWays) override;
    [[nodiscard]] std::unique_ptr<ReplacementPolicy> clone() const override;
    void reset() override; // Add this line

private:
    // Track which ways have been used for each set
    std::unordered_map<uint64_t, std::vector<bool>> usedWays_;
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
