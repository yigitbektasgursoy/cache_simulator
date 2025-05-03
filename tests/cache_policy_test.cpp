// cache_policy_test.cpp
#include "cache_policy.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <set>
#include <map>
#include <random>
#include <algorithm>
#include <memory>

// Test framework macros
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

// Test 1: LRU Policy - Basic Functionality
void testLRUBasicFunctionality() {
    TEST_CASE("LRU Policy - Basic Functionality") {
        LRUPolicy policy;

        // Test 1.1: Empty set behavior
        ASSERT(policy.getVictim(0, 4) == 0); // Should return first way with empty set

        // Test 1.2: Single access behavior
        policy.onAccess(0, 0);  // Access way 0 in set 0
        ASSERT(policy.getVictim(0, 4) == 1); // Should return next unused way

        // Test 1.3: Multiple accesses
        policy.onAccess(0, 1);  // Access way 1 in set 0
        policy.onAccess(0, 2);  // Access way 2 in set 0
        policy.onAccess(0, 3);  // Access way 3 in set 0

        // All ways are used, way 0 is least recently used
        ASSERT(policy.getVictim(0, 4) == 0);

        // Test 1.4: Update LRU order
        policy.onAccess(0, 0);  // Access way 0 again
        ASSERT(policy.getVictim(0, 4) == 1); // Now way 1 is LRU

        // Test 1.5: Different sets are independent
        policy.onAccess(1, 2);  // Access way 2 in set 1
        ASSERT(policy.getVictim(1, 4) == 0); // Should return unused way in set 1
    }
    END_TEST;
}

// Test 2: LRU Policy - Complex Access Patterns
void testLRUComplexPatterns() {
    TEST_CASE("LRU Policy - Complex Access Patterns") {
        LRUPolicy policy;

        // Create a specific access pattern
        std::vector<std::pair<uint64_t, uint64_t>> accesses = {
            {0, 0}, {0, 1}, {0, 2}, {0, 0}, {0, 3}, {0, 1}, {0, 2}, {0, 3}
        };

        for (const auto& access : accesses) {
            policy.onAccess(access.first, access.second);
        }

        // Expected LRU order: 0, 1, 2, 3 (from LRU to MRU)
        ASSERT(policy.getVictim(0, 4) == 0);

        // Test repeated access to same way
        for (int i = 0; i < 5; i++) {
            policy.onAccess(0, 0);
        }
        ASSERT(policy.getVictim(0, 4) == 1); // Way 1 should be LRU now

        // Test interleaved access pattern
        policy.onAccess(0, 2);
        policy.onAccess(0, 1);
        policy.onAccess(0, 3);
        policy.onAccess(0, 0);
        ASSERT(policy.getVictim(0, 4) == 2); // Way 2 should be LRU
    }
    END_TEST;
}

// Test 3: LRU Policy - Edge Cases
// Fixed version of LRU Edge Cases test
void testLRUEdgeCases() {
    TEST_CASE("LRU Policy - Edge Cases") {
        // Test 3.1: Maximum ways
        {
            LRUPolicy policy;  // Create new policy for this test
            const uint64_t maxWays = 1024;
            for (uint64_t i = 0; i < maxWays; i++) {
                policy.onAccess(0, i);
            }
            ASSERT(policy.getVictim(0, maxWays) == 0); // First way should be LRU
        }

        // Test 3.2: Multiple sets with maximum ways
        {
            LRUPolicy policy;  // Create new policy for this test
            for (uint64_t set = 0; set < 10; set++) {
                for (uint64_t way = 0; way < 32; way++) {
                    policy.onAccess(set, way);
                }
            }

            // Verify each set independently
            for (uint64_t set = 0; set < 10; set++) {
                ASSERT(policy.getVictim(set, 32) == 0);
            }
        }

        // Test 3.3: Single way configuration
        {
            LRUPolicy singleWayPolicy;
            singleWayPolicy.onAccess(0, 0);
            ASSERT(singleWayPolicy.getVictim(0, 1) == 0);
        }

        // Test 3.4: Large set numbers
        {
            LRUPolicy policy;
            policy.onAccess(999999, 5);
            ASSERT(policy.getVictim(999999, 8) == 0); // Should return unused way
        }
    }
    END_TEST;
}

// Test 4: FIFO Policy - Basic Functionality
void testFIFOBasicFunctionality() {
    TEST_CASE("FIFO Policy - Basic Functionality") {
        FIFOPolicy policy;

        // Test 4.1: Empty set behavior
        ASSERT(policy.getVictim(0, 4) == 0);

        // Test 4.2: Sequential insertion
        policy.onAccess(0, 0);
        policy.onAccess(0, 1);
        policy.onAccess(0, 2);
        policy.onAccess(0, 3);

        // Way 0 was inserted first, should be evicted first
        ASSERT(policy.getVictim(0, 4) == 0);

        // Test 4.3: Re-access doesn't change FIFO order
        policy.onAccess(0, 0);  // Re-access way 0
        ASSERT(policy.getVictim(0, 4) == 0); // Way 0 should still be first out

        // Test 4.4: Different sets are independent
        policy.onAccess(1, 2);
        policy.onAccess(1, 1);
        // Only 2 ways used out of 4, should return first unused (way 0)
        ASSERT(policy.getVictim(1, 4) == 0);

        // Fill all ways to test actual FIFO behavior
        policy.onAccess(1, 0);
        policy.onAccess(1, 3);
        // Now all 4 ways are used, should return oldest (way 2)
        ASSERT(policy.getVictim(1, 4) == 2);
    }
    END_TEST;
}

// Test 5: FIFO Policy - Complex Patterns
void testFIFOComplexPatterns() {
    TEST_CASE("FIFO Policy - Complex Patterns") {
        FIFOPolicy policy;

        // Test insertion pattern with re-accesses
        policy.onAccess(0, 0);
        policy.onAccess(0, 1);
        policy.onAccess(0, 0);  // Re-access
        policy.onAccess(0, 2);
        policy.onAccess(0, 1);  // Re-access
        policy.onAccess(0, 3);

        // FIFO order should be: 0, 1, 2, 3 (based on first insertion)
        ASSERT(policy.getVictim(0, 4) == 0);

        // Test partial filling
        FIFOPolicy partialPolicy;
        partialPolicy.onAccess(0, 2);
        partialPolicy.onAccess(0, 5);
        // Only 2 ways used out of 8, should return first unused (way 0)
        ASSERT(partialPolicy.getVictim(0, 8) == 0);

        // Fill more ways to test actual FIFO
        partialPolicy.onAccess(0, 0);
        partialPolicy.onAccess(0, 1);
        partialPolicy.onAccess(0, 3);
        partialPolicy.onAccess(0, 4);
        partialPolicy.onAccess(0, 6);
        partialPolicy.onAccess(0, 7);
        // Now all 8 ways are used, should return oldest (way 2)
        ASSERT(partialPolicy.getVictim(0, 8) == 2);

        // Test interleaved sets
        FIFOPolicy interleavedPolicy;
        interleavedPolicy.onAccess(0, 0);
        interleavedPolicy.onAccess(1, 0);
        interleavedPolicy.onAccess(0, 1);
        interleavedPolicy.onAccess(1, 1);

        ASSERT(interleavedPolicy.getVictim(0, 4) == 2);  // First unused in set 0
        ASSERT(interleavedPolicy.getVictim(1, 4) == 2);  // First unused in set 1
    }
    END_TEST;
}

// Test 6: Random Policy - Statistical Behavior
void testRandomPolicyStatistics() {
    TEST_CASE("Random Policy - Statistical Behavior") {
        RandomPolicy policy;
        const int numTrials = 10000;
        const uint64_t numWays = 8;

        // Test uniform distribution
        std::map<uint64_t, int> victimCounts;
        for (int i = 0; i < numTrials; i++) {
            uint64_t victim = policy.getVictim(0, numWays);
            ASSERT(victim < numWays);
            victimCounts[victim]++;
        }

        // Check that all ways are selected
        ASSERT(victimCounts.size() == numWays);

        // Check for reasonable distribution (chi-square test approximation)
        double expectedCount = numTrials / static_cast<double>(numWays);
        double chiSquare = 0.0;

        for (const auto& pair : victimCounts) {
            double diff = pair.second - expectedCount;
            chiSquare += (diff * diff) / expectedCount;
        }

        // With df=7, critical value at 0.05 is approximately 14.07
        ASSERT(chiSquare < 20.0); // Allow some margin

        // Test that onAccess doesn't affect randomness
        for (int i = 0; i < 100; i++) {
            policy.onAccess(0, i % numWays);
        }

        // Should still produce random victims
        std::set<uint64_t> selectedVictims;
        for (int i = 0; i < 20; i++) {
            selectedVictims.insert(policy.getVictim(0, numWays));
        }
        ASSERT(selectedVictims.size() > 1); // Should have variety
    }
    END_TEST;
}

// Test 7: Policy Cloning
void testPolicyCloning() {
    TEST_CASE("Policy Cloning") {
        // Test LRU cloning
        LRUPolicy lruOriginal;
        lruOriginal.onAccess(0, 0);
        lruOriginal.onAccess(0, 1);
        lruOriginal.onAccess(0, 2);

        auto lruClone = lruOriginal.clone();
        ASSERT(lruClone->getVictim(0, 3) == 0);

        // Modify original, check clone is independent
        lruOriginal.onAccess(0, 0);
        ASSERT(lruClone->getVictim(0, 3) == 0); // Still the same
        ASSERT(lruOriginal.getVictim(0, 3) == 1); // Changed

        // Test FIFO cloning
        FIFOPolicy fifoOriginal;
        fifoOriginal.onAccess(0, 2);
        fifoOriginal.onAccess(0, 1);

        auto fifoClone = fifoOriginal.clone();
        // Only 2 ways used out of 3, should return first unused (way 0)
        ASSERT(fifoClone->getVictim(0, 3) == 0);

        // Add one more way to test actual FIFO
        fifoOriginal.onAccess(0, 0);
        fifoClone = fifoOriginal.clone();
        // Now all 3 ways are used, should return oldest (way 2)
        ASSERT(fifoClone->getVictim(0, 3) == 2);

        // Test Random cloning
        RandomPolicy randomOriginal;
        auto randomClone = randomOriginal.clone();
        // Both should produce valid victims
        ASSERT(randomClone->getVictim(0, 4) < 4);
        ASSERT(randomOriginal.getVictim(0, 4) < 4);
    }
    END_TEST;
}

// Test 8: Policy Factory
void testPolicyFactory() {
    TEST_CASE("Policy Factory") {
        // Test creation by name
        auto lruPolicy = createPolicy("LRU");
        ASSERT(dynamic_cast<LRUPolicy*>(lruPolicy.get()) != nullptr);

        auto fifoPolicy = createPolicy("FIFO");
        ASSERT(dynamic_cast<FIFOPolicy*>(fifoPolicy.get()) != nullptr);

        auto randomPolicy = createPolicy("RANDOM");
        ASSERT(dynamic_cast<RandomPolicy*>(randomPolicy.get()) != nullptr);

        // Test unknown policy defaults to LRU
        auto unknownPolicy = createPolicy("UNKNOWN");
        ASSERT(dynamic_cast<LRUPolicy*>(unknownPolicy.get()) != nullptr);

        // Test created policies work correctly
        lruPolicy->onAccess(0, 0);
        ASSERT(lruPolicy->getVictim(0, 4) == 1);

        fifoPolicy->onAccess(0, 3);
        // Only 1 way used out of 4, should return first unused (way 0)
        ASSERT(fifoPolicy->getVictim(0, 4) == 0);

        // Fill all ways to test actual FIFO
        fifoPolicy->onAccess(0, 0);
        fifoPolicy->onAccess(0, 1);
        fifoPolicy->onAccess(0, 2);
        // Now all 4 ways are used, should return oldest (way 3)
        ASSERT(fifoPolicy->getVictim(0, 4) == 3);
    }
    END_TEST;
}

// Test 9: Stress Test - Large Scale Operations
void testStressTest() {
    TEST_CASE("Stress Test - Large Scale Operations") {
        const uint64_t numSets = 1000;
        const uint64_t numWays = 64;
        const int numOperations = 100000;

        // Create policies
        LRUPolicy lru;
        FIFOPolicy fifo;
        RandomPolicy random;

        std::mt19937 rng(42);
        std::uniform_int_distribution<uint64_t> setDist(0, numSets - 1);
        std::uniform_int_distribution<uint64_t> wayDist(0, numWays - 1);

        // Perform random operations
        for (int i = 0; i < numOperations; i++) {
            uint64_t set = setDist(rng);
            uint64_t way = wayDist(rng);

            lru.onAccess(set, way);
            fifo.onAccess(set, way);
            random.onAccess(set, way);

            // Occasionally check victims
            if (i % 1000 == 0) {
                uint64_t lruVictim = lru.getVictim(set, numWays);
                uint64_t fifoVictim = fifo.getVictim(set, numWays);
                uint64_t randomVictim = random.getVictim(set, numWays);

                ASSERT(lruVictim < numWays);
                ASSERT(fifoVictim < numWays);
                ASSERT(randomVictim < numWays);
            }
        }

        // All policies should still work correctly
        for (uint64_t set = 0; set < 10; set++) {
            ASSERT(lru.getVictim(set, numWays) < numWays);
            ASSERT(fifo.getVictim(set, numWays) < numWays);
            ASSERT(random.getVictim(set, numWays) < numWays);
        }
    }
    END_TEST;
}

// Test 10: Edge Cases and Error Conditions
void testEdgeCasesAndErrors() {
    TEST_CASE("Edge Cases and Error Conditions") {
        LRUPolicy lru;
        FIFOPolicy fifo;
        RandomPolicy random;

        // Test with maximum uint64_t values
        uint64_t maxSet = std::numeric_limits<uint64_t>::max();
        uint64_t maxWay = 100; // Use reasonable max way

        lru.onAccess(maxSet, 0);
        ASSERT(lru.getVictim(maxSet, maxWay) == 1);

        fifo.onAccess(maxSet, 0);
        // Only 1 way used out of 100, should return first unused (way 1)
        ASSERT(fifo.getVictim(maxSet, maxWay) == 1);

        // Fill more ways to test actual FIFO
        for (uint64_t i = 1; i < maxWay; i++) {
            fifo.onAccess(maxSet, i);
        }
        // Now all ways are used, should return oldest (way 0)
        ASSERT(fifo.getVictim(maxSet, maxWay) == 0);

        ASSERT(random.getVictim(maxSet, maxWay) < maxWay);

        // Test consistency after many operations
        for (int i = 0; i < 1000; i++) {
            lru.onAccess(0, i % 8);
            fifo.onAccess(0, i % 8);
        }

        // Should still produce valid victims
        ASSERT(lru.getVictim(0, 8) < 8);
        ASSERT(fifo.getVictim(0, 8) < 8);
    }
    END_TEST;
}

// Main test runner
int main() {
    std::cout << "Running Cache Policy test suite..." << std::endl;
    std::cout << "==================================" << std::endl;

    // LRU Policy tests
    testLRUBasicFunctionality();
    testLRUComplexPatterns();
    testLRUEdgeCases();

    // FIFO Policy tests
    testFIFOBasicFunctionality();
    testFIFOComplexPatterns();

    // Random Policy tests
    testRandomPolicyStatistics();

    // Common tests
    testPolicyCloning();
    testPolicyFactory();
    testStressTest();
    testEdgeCasesAndErrors();

    std::cout << "\nAll tests completed." << std::endl;
    return 0;
}
