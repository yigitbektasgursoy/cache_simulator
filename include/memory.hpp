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
