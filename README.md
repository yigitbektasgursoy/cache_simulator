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
-  Non-Inclusive Non-Exclusive (NINE) is not fully implemented
### Memory Trace Sources
- **File-based traces**: Support for custom trace file formats
- **Synthetic traces**: Algorithmic pattern generation
  - Sequential access patterns
  - Random access patterns
  - Strided access patterns (matrix-like)
  - Looping patterns (temporal locality)

### Performance Analysis
- **Comprehensive Metrics**: Hit rates, miss rates, AMAT (Average Memory Access Time)
- **Comparison Tools**: Multi-configuration performance comparison
- **Export Options**: CSV data export for analysis with external tools
- **Detailed Statistics**: Per-cache-level performance breakdown

## Building and Installation

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- CMake 3.20 or higher
- nlohmann/json library (automatically fetched during build)

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

### Command Line Options
```bash
# Run with a single configuration
./cache_simulator config.json

# Compare multiple configurations
./cache_simulator config1.json config2.json config3.json --compare

# Save results to CSV for external analysis
./cache_simulator config.json --csv

# Verbose output during simulation
./cache_simulator config.json --verbose

# Compare multiple configs and save to CSV
./cache_simulator config1.json config2.json --compare --csv

# Display help
./cache_simulator --help
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
- `level`: Cache level (1 for L1, 2 for L2, etc.)
- `organization`: "DirectMapped", "SetAssociative", "FullyAssociative"
- `size`: Total cache size in bytes
- `block_size`: Cache block size in bytes
- `associativity`: Number of ways (for set-associative caches)
- `policy`: "LRU", "FIFO", "RANDOM"
- `access_latency`: Access time in cycles
- `write_back`: true for write-back, false for write-through
- `write_allocate`: true for write-allocate, false for no-write-allocate
- `inclusion_policy`: "Inclusive", "Exclusive", "NINE" (L2+ only)

#### Memory Configuration
- `access_latency`: Main memory access latency in cycles

#### Trace Configuration
- **File traces**: 
  ```json
  {
    "type": "File",
    "filename": "trace.txt"
  }
  ```
- **Synthetic traces**: 
  ```json
  {
    "type": "Synthetic",
    "pattern": "Sequential|Random|Strided|Looping",
    "start_address": 0,
    "end_address": 1048576,
    "num_accesses": 10000,
    "read_ratio": 0.7
  }
  ```

## Architecture

### Core Components
- **MemoryAddress**: Address manipulation with bit field extraction
- **Cache**: Generic cache implementation with configurable policies
- **CacheHierarchy**: Multi-level cache management with inclusion policy support
- **ReplacementPolicy**: Pluggable replacement algorithms (LRU, FIFO, Random)
- **MemoryTraceSource**: Flexible trace input system (File and Synthetic)
- **PerformanceAnalyzer**: Metrics collection and analysis
- **JsonConfigLoader**: Configuration file parsing and validation

### Key Features
- **Modern C++20**: Uses concepts, structured bindings, and other modern features
- **Policy-based Design**: Easily extensible replacement policies
- **Comprehensive Analysis**: Built-in performance analysis and comparison tools
- **Flexible Configuration**: JSON-based configuration with validation
- **Multi-level Support**: Supports complex cache hierarchies with different inclusion policies

## Example Experiments

### Basic Cache Analysis
```bash
# Create a simple L1 cache configuration
./cache_simulator basic_l1_config.json --verbose
```

### L1 Cache Size Comparison
```bash
# Compare different L1 cache sizes
./cache_simulator l1_32k.json l1_64k.json l1_128k.json --compare --csv
```

### Replacement Policy Analysis
```bash
# Compare LRU, FIFO, and Random policies
./cache_simulator lru_config.json fifo_config.json random_config.json --compare
```

### Multi-level Cache Analysis
```bash
# Analyze L1+L2 hierarchy with different inclusion policies
./cache_simulator inclusive_l2.json exclusive_l2.json nine_l2.json --compare --csv
```

### Trace Pattern Comparison
```bash
# Compare different access patterns
./cache_simulator sequential_trace.json random_trace.json strided_trace.json --compare
```

## Performance Metrics

The simulator provides comprehensive performance analysis:

- **Hit Rates**: Per-level cache hit rates and miss rates
- **AMAT**: Average Memory Access Time with per-level contributions
- **Access Counts**: Detailed hit/miss statistics for each cache level
- **Memory Traffic**: Main memory read/write statistics
- **Latency Breakdown**: Contribution analysis per cache level
- **Inclusion Policy Effects**: Analysis of different inclusion policies

### Sample Output
```
Cache Statistics:
--------------------------------------------------
L1 Cache:
  Hits:   7842
  Misses: 2158
  Total:  10000
  Hit Rate: 78.42%
  Configuration:
    Size: 32768 bytes
    Block Size: 64 bytes
    Associativity: 8 ways
    Sets: 64
    Replacement Policy: LRU

L2 Cache:
  Hits:   1876
  Misses: 282
  Total:  2158
  Hit Rate: 86.93%
  Configuration:
    Size: 262144 bytes
    Block Size: 64 bytes
    Associativity: 16 ways
    Sets: 256
    Replacement Policy: LRU
    Inclusion Policy: Inclusive

Memory Statistics:
--------------------------------------------------
  Reads:  225
  Writes: 57
  Total:  282
  Latency: 100 cycles
```

## Output Files

When using the `--csv` option, the simulator generates:
- `cache_comparison_results.csv` - When comparing multiple configurations
- `{test_name}_results.csv` - When analyzing a single configuration

These CSV files can be imported into Excel, Python (pandas), R, or other analysis tools for further visualization and analysis.

## Future Work

Several enhancement opportunities exist for expanding the cache simulator capabilities:

### 🚀 **Implementation Scalability**
- **Streaming Trace Processing**: Implement pipe support and streaming to handle large trace files (45GB+) without requiring complete download and storage
- **Distributed Processing**: Enable distributed simulation across multiple nodes for larger parameter spaces

### 📊 **Advanced Analysis**
- **Timing Integration**: Integrate with CACTI or similar tools for realistic SRAM delay models and AMAT analysis
- **Power and Area Analysis**: Add power and area models for comprehensive design space exploration

### 🏗️ **Architecture Extensions**
- **Advanced Replacement Policies**: Implement pseudo-LRU, tree-based, and adaptive replacement strategies
- **Three-Level Hierarchy**: Add L3 cache support to align with modern processor architectures
- **Multi-core and Coherence**: Extend to multi-core configurations with coherence protocol analysis

### 📈 **Benchmark and Workload Expansion**
- **Modern Benchmarks**: Add machine learning workloads, graph algorithms, and contemporary applications
- **Real-world Traces**: Integrate production application traces for actual deployment scenario analysis

### 🔧 **Current Limitations**
- Large trace file management (45GB+ storage requirements)
- Two-level hierarchy limitation vs. modern 3+ level caches
- Limited to three replacement policies (LRU, FIFO, Random)
- Non-timing metrics focus (timing models planned for future integration)

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- New replacement policies
- Additional trace formats
- Performance optimizations
- Documentation improvements
- Future work implementations listed above
