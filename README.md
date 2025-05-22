# Cache Simulator

A comprehensive, configurable cache simulator written in C++20 for analyzing memory hierarchy performance in computer systems.

## Features

### Cache Configurations
- **Cache Levels**: L1, L2, and multi-level hierarchies
- **Organization Types**: 
  - Direct-mapped
  - Set-associative (configurable associativity)
  - Fully associative
- **Replacement Policies**: LRU, FIFO, Random
- **Write Policies**: Write-back/Write-through, Write-allocate/No-write-allocate
- **Inclusion Policies**: Inclusive, Exclusive, Non-Inclusive Non-Exclusive (NINE)

### Memory Trace Sources
- **File-based traces**: Support for custom trace file formats
- **Synthetic traces**: Algorithmic pattern generation
  - Sequential access patterns
  - Random access patterns
  - Strided access patterns (matrix-like)
  - Looping patterns (temporal locality)

### Performance Analysis
- **Comprehensive Metrics**: Hit rates, miss rates, AMAT (Average Memory Access Time)
- **Visualization**: Automatic generation of Python plotting scripts
- **Comparison Tools**: Multi-configuration performance comparison
- **Export Options**: CSV data export for further analysis

## Building and Installation

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- CMake 3.16 or higher
- nlohmann/json library

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd cache-simulator

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)
```

## Usage

### Basic Usage

```bash
# Run with a single configuration
./cache_simulator config.json

# Compare multiple configurations
./cache_simulator config1.json config2.json config3.json --compare

# Generate visualizations
./cache_simulator config.json --visualize

# Verbose output
./cache_simulator config.json --verbose
```

### Configuration File Format

The simulator uses JSON configuration files to define cache hierarchies and test parameters:

```json
{
    "test_name": "L1_Cache_Test",
    "cache_hierarchy": [
        {
            "level": 1,
            "organization": "SetAssociative",
            "size": 65536,
            "block_size": 64,
            "associativity": 8,
            "policy": "LRU",
            "access_latency": 1,
            "write_back": true,
            "write_allocate": true
        },
        {
            "level": 2,
            "organization": "SetAssociative",
            "size": 1048576,
            "block_size": 64,
            "associativity": 16,
            "policy": "LRU",
            "access_latency": 10,
            "write_back": true,
            "write_allocate": true,
            "inclusion_policy": "Inclusive"
        }
    ],
    "memory": {
        "access_latency": 100
    },
    "trace": {
        "type": "Synthetic",
        "pattern": "Random",
        "start_address": 0,
        "end_address": 1048576,
        "num_accesses": 100000,
        "read_ratio": 0.7
    }
}
```

### Configuration Parameters

#### Cache Configuration
- `organization`: "DirectMapped", "SetAssociative", "FullyAssociative"
- `size`: Total cache size in bytes
- `block_size`: Cache block size in bytes
- `associativity`: Number of ways (for set-associative caches)
- `policy`: "LRU", "FIFO", "RANDOM"
- `access_latency`: Access time in cycles
- `write_back`: true for write-back, false for write-through
- `write_allocate`: true for write-allocate, false for no-write-allocate
- `inclusion_policy`: "Inclusive", "Exclusive", "NINE" (L2+ only)

#### Trace Configuration
- **File traces**: `{"type": "File", "filename": "trace.txt"}`
- **Synthetic traces**: Pattern-based generation with configurable parameters

## Architecture

### Core Components

- **MemoryAddress**: Address manipulation with bit field extraction
- **Cache**: Generic cache implementation with configurable policies
- **CacheHierarchy**: Multi-level cache management
- **ReplacementPolicy**: Pluggable replacement algorithms
- **MemoryTraceSource**: Flexible trace input system
- **PerformanceAnalyzer**: Metrics collection and analysis

### Key Features

- **Modern C++20**: Uses concepts, ranges, and other modern features
- **Policy-based Design**: Easily extensible replacement policies
- **Comprehensive Testing**: Built-in performance analysis and comparison tools
- **Visualization Ready**: Automatic generation of plotting scripts

## Example Experiments

### L1 Cache Size Analysis
```bash
# Generate configurations with varying L1 sizes
./cache_simulator configs/01_L1_Size_Variations/*.json --compare --visualize
```

### Replacement Policy Comparison
```bash
# Compare LRU, FIFO, and Random policies
./cache_simulator configs/04_L1_Policy_Variations/*.json --compare --visualize
```

### Multi-level Cache Analysis
```bash
# Analyze L1+L2 hierarchy with different inclusion policies
./cache_simulator configs/inclusion_policies/*.json --compare --visualize
```

## Performance Metrics

The simulator provides comprehensive performance analysis:

- **Hit Rates**: Per-level cache hit rates
- **AMAT**: Average Memory Access Time calculation
- **Memory Traffic**: Read/write statistics
- **Latency Breakdown**: Contribution analysis per cache level
