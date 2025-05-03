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
