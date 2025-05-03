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
