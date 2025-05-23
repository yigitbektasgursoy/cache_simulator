#!/usr/bin/env python3
import json
import os
import copy

# --- Determine Project Root and other key paths ---
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
CONFIGS_DIR = os.path.join(PROJECT_ROOT, "configs")
TRACES_DIR = os.path.join(PROJECT_ROOT, "traces")

os.makedirs(CONFIGS_DIR, exist_ok=True)
os.makedirs(TRACES_DIR, exist_ok=True)

# --- Base Configuration Template ---
# Note: We still include timing fields for simulator compatibility, but analysis won't rely on them
base_config_template = {
    "test_name_base_suffix": "DefaultL1L2",
    "cache_hierarchy": [
        {
            "level": 1, "organization": "SetAssociative", "size": 1024,
            "block_size": 32, "associativity": 2, "policy": "LRU",
            "access_latency": 1, # Keep for compatibility, but won't be used in analysis
            "write_back": True, "write_allocate": True
        },
        {
            "level": 2, "organization": "SetAssociative", "size": 8192,
            "block_size": 32, "associativity": 4, "policy": "LRU",
            "access_latency": 10, # Keep for compatibility, but won't be used in analysis
            "write_back": True, "write_allocate": True,
            "inclusion_policy": "Inclusive"
        }
    ],
    "memory": {"access_latency": 100}, # Keep for compatibility
    "trace": {"type": "File", "filename": ""},
    "description": ""
}

# --- L1-only Configuration Template ---
l1_only_config_template = {
    "test_name_base_suffix": "SingleLevelL1",
    "cache_hierarchy": [
        {
            "level": 1, "organization": "SetAssociative", "size": 1024,
            "block_size": 32, "associativity": 2, "policy": "LRU",
            "access_latency": 1, # Keep for compatibility
            "write_back": True, "write_allocate": True
        }
    ],
    "memory": {"access_latency": 100}, # Keep for compatibility
    "trace": {"type": "File", "filename": ""},
    "description": ""
}

# --- BENCHMARKS: Focus on diverse access patterns for algorithmic analysis ---
BENCHMARKS = {
    "SequentialAccess": {"trace_file": "sequential_access.out", "description": "Sequential memory access pattern"},
    "StridedAccess": {"trace_file": "strided_access.out", "description": "Strided memory access pattern"},
    "RowMajorMatrix": {"trace_file": "row_major.out", "description": "Row-major matrix traversal"},
    "ColumnMajorMatrix": {"trace_file": "column_major.out", "description": "Column-major matrix traversal"},
    "RandomAccess": {"trace_file": "random_access.out", "description": "Random memory access pattern"},
    "LinkedListTraversal": {"trace_file": "linked_list.out", "description": "Linked list traversal"},
    "BlockedDataAccess": {"trace_file": "blocked_access.out", "description": "Blocked data access pattern"},
    "WriteHeavy": {"trace_file": "write_heavy.out", "description": "Write-intensive workload"},
    "MatrixNaive": {"trace_file": "matrix_naive.out", "description": "Naive matrix multiplication"},
    "MatrixBlocked": {"trace_file": "matrix_blocked.out", "description": "Blocked matrix multiplication"},
    "BinaryTree": {"trace_file": "binary_tree.out", "description": "Binary tree traversal"},
    "ConflictHeavy": {"trace_file": "conflict_heavy.out", "description": "Conflict-heavy access pattern"},
}

BENCHMARK_GROUPS = {
    "all": list(BENCHMARKS.keys()),
    "quick_test": ["SequentialAccess", "RandomAccess", "LinkedListTraversal"],
    "spatial_locality": ["SequentialAccess", "RowMajorMatrix", "BlockedDataAccess"],
    "temporal_locality": ["LinkedListTraversal", "BinaryTree", "ConflictHeavy"],
    "mixed_patterns": ["StridedAccess", "ColumnMajorMatrix", "WriteHeavy"]
}

def get_num_blocks(size_bytes, block_size_bytes):
    if not isinstance(size_bytes, int) or not isinstance(block_size_bytes, int): return 0
    if block_size_bytes <= 0: return 0
    return size_bytes // block_size_bytes

def get_cache_level_org_details(cache_level_config):
    """
    Updates 'organization' and 'associativity' (caps it) in cache_level_config.
    Returns a dictionary: {'label': str, 'org_string': str, 'eff_assoc': int}
    """
    size_val = cache_level_config.get("size", 0)
    block_size_val = cache_level_config.get("block_size", 0)
    assoc_val = cache_level_config.get("associativity", 1)

    details = {'label': "UnknownOrg", 'org_string': "SetAssociative", 'eff_assoc': assoc_val}

    if block_size_val <= 0:
        details.update({'label': "InvalidBlock", 'org_string': "Invalid_BlockSize", 'eff_assoc': 0})
        cache_level_config["organization"] = details['org_string']
        cache_level_config["associativity"] = details['eff_assoc']
        return details

    num_blocks = get_num_blocks(size_val, block_size_val)
    if num_blocks == 0:
        details.update({'label': "InvalidSizeOrBlock", 'org_string': "Invalid_ZeroBlocks", 'eff_assoc': 0})
        cache_level_config["organization"] = details['org_string']
        cache_level_config["associativity"] = details['eff_assoc']
        return details

    actual_assoc = min(assoc_val, num_blocks)
    cache_level_config["associativity"] = actual_assoc

    if actual_assoc >= num_blocks:
        details.update({'label': "FullyAssoc", 'org_string': "FullyAssociative", 'eff_assoc': actual_assoc})
    elif actual_assoc == 1:
        details.update({'label': "DirectMapped", 'org_string': "DirectMapped", 'eff_assoc': actual_assoc})
    else:
        details.update({'label': f"{actual_assoc}way", 'org_string': "SetAssociative", 'eff_assoc': actual_assoc})

    cache_level_config["organization"] = details['org_string']
    return details

# === PHASE 1: SINGLE CACHE ANALYSIS (Hit Rate Focus) ===

def generate_base_configs():
    """Generates the base configuration for baseline comparison"""
    config = copy.deepcopy(base_config_template)
    config["variation_tag"] = "BaseConfig"
    return [config]

def vary_l1_cache_size():
    """Vary L1 cache size to study capacity impact on hit rates"""
    configs = []
    # Wider range of sizes to see capacity effects clearly
    l1_sizes = [256, 512, 1024, 2048, 4096, 8192, 16384]
    for size_val in l1_sizes:
        config = copy.deepcopy(base_config_template)
        size_kb_str = f"{size_val//1024}KB" if size_val >= 1024 else f"{size_val}B"
        config["cache_hierarchy"][0]["size"] = size_val
        config["variation_tag"] = f"L1Size_{size_kb_str}"
        configs.append(config)
    return configs

def vary_l1_associativity():
    """Vary L1 associativity to study conflict miss reduction"""
    configs = []
    assoc_targets = [1, 2, 4, 8, 16, 32]
    base_l1_size = base_config_template["cache_hierarchy"][0]["size"]
    base_l1_block_size = base_config_template["cache_hierarchy"][0]["block_size"]
    max_assoc_for_base_l1 = get_num_blocks(base_l1_size, base_l1_block_size)
    if max_assoc_for_base_l1 > 0: assoc_targets.append(max_assoc_for_base_l1)
    unique_assoc_targets = sorted(list(set(assoc_targets)))

    for target_assoc in unique_assoc_targets:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["associativity"] = target_assoc
        # Create a temporary copy for get_cache_level_org_details not to modify the original 'config' prematurely
        temp_config_for_details = copy.deepcopy(config)
        temp_l1_details = get_cache_level_org_details(temp_config_for_details["cache_hierarchy"][0])
        config["variation_tag"] = f"L1Assoc_{temp_l1_details['label']}"
        configs.append(config)
    return configs

def vary_l1_block_size():
    """Vary L1 block size to study spatial locality exploitation"""
    configs = []
    l1_block_sizes = [16, 32, 64, 128, 256]  # Wider range to see spatial locality effects
    for bs_val in l1_block_sizes:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["block_size"] = bs_val
        # Keep L2 block size >= L1 block size for hierarchy compatibility
        if len(config["cache_hierarchy"]) > 1:
            config["cache_hierarchy"][1]["block_size"] = max(bs_val, config["cache_hierarchy"][1].get("block_size", bs_val))
        config["variation_tag"] = f"L1Block_{bs_val}B"
        configs.append(config)
    return configs

def vary_l1_replacement_policy():
    """Compare replacement policy effectiveness"""
    configs = []
    policies = ["LRU", "FIFO", "RANDOM"]
    for policy_val in policies:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["policy"] = policy_val
        config["variation_tag"] = f"L1Policy_{policy_val}"
        configs.append(config)
    return configs

# === PHASE 2: MULTI-LEVEL ANALYSIS (Hierarchy Hit Rate Focus) ===

def vary_l2_cache_size():
    """Vary L2 cache size to study hierarchy capacity effects"""
    configs = []
    l2_sizes = [2048, 4096, 8192, 16384, 32768, 65536]  # 2KB to 64KB
    for size_val in l2_sizes:
        config = copy.deepcopy(base_config_template)
        size_kb_str = f"{size_val//1024}KB" if size_val >= 1024 else f"{size_val}B"
        config["cache_hierarchy"][1]["size"] = size_val
        config["variation_tag"] = f"L2Size_{size_kb_str}"
        configs.append(config)
    return configs

def vary_l2_associativity():
    """Vary L2 associativity to study L2 conflict behavior"""
    configs = []
    assoc_targets = [1, 2, 4, 8, 16, 32]
    base_l2_size = base_config_template["cache_hierarchy"][1]["size"]
    base_l2_block_size = base_config_template["cache_hierarchy"][1]["block_size"]
    max_assoc_for_base_l2 = get_num_blocks(base_l2_size, base_l2_block_size)
    if max_assoc_for_base_l2 > 0: assoc_targets.append(max_assoc_for_base_l2)
    unique_assoc_targets = sorted(list(set(assoc_targets)))

    for target_assoc in unique_assoc_targets:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][1]["associativity"] = target_assoc
        # Create a temporary copy for get_cache_level_org_details
        temp_config_for_details = copy.deepcopy(config)
        temp_l2_details = get_cache_level_org_details(temp_config_for_details["cache_hierarchy"][1])
        config["variation_tag"] = f"L2Assoc_{temp_l2_details['label']}"
        configs.append(config)
    return configs

def vary_l2_block_size():
    """Vary L2 block size and adjust L1 block size accordingly."""
    configs = []
    l2_block_sizes = [16, 32, 64, 128, 256]
    default_l1_block_size = base_config_template["cache_hierarchy"][0]["block_size"]

    for l2_bs_val in l2_block_sizes:
        config = copy.deepcopy(base_config_template)
        
        # Set L2 block size
        config["cache_hierarchy"][1]["block_size"] = l2_bs_val
        
        # Adjust L1 block size to be <= L2 block size
        # L1 block size should not be larger than its default or the new L2 block size
        new_l1_bs = min(default_l1_block_size, l2_bs_val)
        config["cache_hierarchy"][0]["block_size"] = new_l1_bs
        
        config["variation_tag"] = f"L2Block_{l2_bs_val}B"
        # The description will later reflect the actual L1 block size used.
        configs.append(config)
    return configs

def vary_inclusion_policy():
    """Compare inclusion policies for capacity utilization"""
    configs = []
    inclusion_policies = ["Inclusive", "Exclusive", "NINE"]
    for policy in inclusion_policies:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][1]["inclusion_policy"] = policy
        config["variation_tag"] = f"Inclusion_{policy}"
        configs.append(config)
    return configs

def vary_hierarchy_size_ratios():
    """Study optimal L1:L2 size ratios for hit rate optimization"""
    configs = []
    # Fixed total budget approach - focus on hit rate optimization
    total_budget = 12288  # 12KB total
    ratios = [
        {"l1": 1024, "l2": 11264, "tag": "Small_L1_Large_L2"},     # 1:11 ratio
        {"l1": 2048, "l2": 10240, "tag": "L1_L2_1to5"},           # 1:5 ratio  
        {"l1": 3072, "l2": 9216, "tag": "L1_L2_1to3"},            # 1:3 ratio
        {"l1": 4096, "l2": 8192, "tag": "L1_L2_1to2"},            # 1:2 ratio
        {"l1": 6144, "l2": 6144, "tag": "Equal_L1_L2"},           # 1:1 ratio
        {"l1": 8192, "l2": 4096, "tag": "L1_L2_2to1"},            # 2:1 ratio
        {"l1": 10240, "l2": 2048, "tag": "Large_L1_Small_L2"},    # 5:1 ratio
    ]
    
    for ratio in ratios:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["size"] = ratio["l1"]
        config["cache_hierarchy"][1]["size"] = ratio["l2"]
        config["variation_tag"] = f"HierarchyRatio_{ratio['tag']}"
        configs.append(config)
    return configs

# === PHASE 3: ADVANCED ANALYSIS ===

def vary_l1_only_vs_hierarchy():
    """Compare L1-only vs L1+L2 for same total capacity"""
    configs = []
    
    # L1-only configurations
    l1_only_sizes = [4096, 8192, 12288, 16384]  # Single large L1
    for size_val in l1_only_sizes:
        config = copy.deepcopy(l1_only_config_template)
        size_kb_str = f"{size_val//1024}KB"
        config["cache_hierarchy"][0]["size"] = size_val
        config["variation_tag"] = f"L1Only_{size_kb_str}"
        configs.append(config)
    
    # Equivalent hierarchical configurations
    hierarchy_configs = [
        {"l1": 1024, "l2": 3072, "total": "4KB", "tag": "L1L2_4KB"},
        {"l1": 2048, "l2": 6144, "total": "8KB", "tag": "L1L2_8KB"}, 
        {"l1": 3072, "l2": 9216, "total": "12KB", "tag": "L1L2_12KB"},
        {"l1": 4096, "l2": 12288, "total": "16KB", "tag": "L1L2_16KB"},
    ]
    
    for hconfig in hierarchy_configs:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["size"] = hconfig["l1"]
        config["cache_hierarchy"][1]["size"] = hconfig["l2"]
        config["variation_tag"] = f"Hierarchy_{hconfig['tag']}"
        configs.append(config)
        
    return configs

def vary_write_traffic_analysis():
    """Analyze write policies for memory traffic (not timing)"""
    configs = []
    write_combinations = [
        {"write_back": True, "write_allocate": True, "tag": "WB_WA"},
        {"write_back": True, "write_allocate": False, "tag": "WB_NoWA"},
        {"write_back": False, "write_allocate": True, "tag": "WT_WA"},
        {"write_back": False, "write_allocate": False, "tag": "WT_NoWA"}
    ]
    
    for combo in write_combinations:
        config = copy.deepcopy(base_config_template)
        # Apply to both L1 and L2
        if len(config["cache_hierarchy"]) > 0:
            config["cache_hierarchy"][0]["write_back"] = combo["write_back"]
            config["cache_hierarchy"][0]["write_allocate"] = combo["write_allocate"]
        if len(config["cache_hierarchy"]) > 1:
            config["cache_hierarchy"][1]["write_back"] = combo["write_back"]
            config["cache_hierarchy"][1]["write_allocate"] = combo["write_allocate"]
        config["variation_tag"] = f"WriteTraffic_{combo['tag']}"
        configs.append(config)
    return configs

def vary_policy_combinations():
    """Study different replacement policies at L1 vs L2"""
    configs = []
    policy_combos = [
        {"l1": "LRU", "l2": "LRU", "tag": "LRU_LRU"},
        {"l1": "LRU", "l2": "FIFO", "tag": "LRU_FIFO"},
        {"l1": "LRU", "l2": "RANDOM", "tag": "LRU_RANDOM"},
        {"l1": "FIFO", "l2": "LRU", "tag": "FIFO_LRU"},
        {"l1": "RANDOM", "l2": "LRU", "tag": "RANDOM_LRU"},
    ]
    
    for combo in policy_combos:
        config = copy.deepcopy(base_config_template)
        if len(config["cache_hierarchy"]) > 0:
            config["cache_hierarchy"][0]["policy"] = combo["l1"]
        if len(config["cache_hierarchy"]) > 1:
            config["cache_hierarchy"][1]["policy"] = combo["l2"]
        config["variation_tag"] = f"PolicyCombo_{combo['tag']}"
        configs.append(config)
    return configs

def save_config_file(config_data, subdir_name, filename):
    safe_filename = "".join(c if c.isalnum() or c in ('_', '-') else '_' for c in filename)
    full_subdir_path = os.path.join(CONFIGS_DIR, subdir_name)
    os.makedirs(full_subdir_path, exist_ok=True)
    filepath = os.path.join(full_subdir_path, f"{safe_filename}.json")
    try:
        with open(filepath, 'w') as f:
            json.dump(config_data, f, indent=2)
    except Exception as e:
        print(f"Error saving {filepath}: {e}")
    return filepath

def check_trace_files_exist():
    all_exist = True
    print("Checking for trace files...")
    for key, info in BENCHMARKS.items():
        trace_path = os.path.join(TRACES_DIR, info["trace_file"])
        if not os.path.exists(trace_path):
            print(f"  MISSING: {trace_path} (for benchmark '{key}')")
            all_exist = False
        else:
            print(f"  FOUND: {trace_path}")
    if not all_exist:
        print("\nWARNING: Some trace files are missing. Please ensure they exist.")
        print("The generator will still create configs, but you'll need trace files to run experiments.")
    return all_exist

def main():
    print(f"--- Cache Configuration Generator (Hit Rate Focus) ---")
    print(f"Project Root: {PROJECT_ROOT}")
    print(f"Config Output Directory: {CONFIGS_DIR}")
    print(f"Trace Directory: {TRACES_DIR}\n")
    print("NOTE: This generator focuses on hit rate and algorithmic analysis,")
    print("      avoiding timing-dependent experiments that require accurate latency models.\n")

    check_trace_files_exist()

    # Define experiment sets focused on algorithmic cache behavior
    variation_sets = {
        # === PHASE 1: SINGLE CACHE ANALYSIS ===
        "00_Base_Configs": generate_base_configs,
        "01_L1_Size_Variations": vary_l1_cache_size,
        "02_L1_Associativity_Variations": vary_l1_associativity,       
        "03_L1_BlockSize_Variations": vary_l1_block_size,
        "04_L1_Policy_Variations": vary_l1_replacement_policy,
        
        # === PHASE 2: HIERARCHY ANALYSIS ===
        "05_L2_Size_Variations": vary_l2_cache_size,
        "06_L2_Associativity_Variations": vary_l2_associativity,
        "07_L2_BlockSize_Variations": vary_l2_block_size, # <-- ADDED HERE
        "08_Inclusion_Policy_Variations": vary_inclusion_policy, # Renumbered
        "09_Hierarchy_Size_Ratios": vary_hierarchy_size_ratios, # Renumbered
        
        # === PHASE 3: ADVANCED COMPARISONS ===
        "10_L1Only_vs_Hierarchy": vary_l1_only_vs_hierarchy, # Renumbered
        "11_Write_Traffic_Analysis": vary_write_traffic_analysis, # Renumbered
        "12_Policy_Combinations": vary_policy_combinations, # Renumbered
    }

    # Use all benchmarks to get comprehensive workload coverage
    benchmarks_to_run_against = BENCHMARK_GROUPS["all"]
    total_files_generated = 0

    for subdir_name, generator_func in variation_sets.items():
        print(f"\nGenerating for experiment set: {subdir_name}")
        param_varied_configs = generator_func()
        count_for_set = 0

        for base_varied_config in param_varied_configs:
            for bench_key in benchmarks_to_run_against:
                bench_details = BENCHMARKS[bench_key]
                
                final_config = copy.deepcopy(base_varied_config)

                # Set trace file
                final_config["trace"]["filename"] = os.path.join(TRACES_DIR, bench_details["trace_file"])

                # Finalize cache organization details
                # This needs to be done after all parameters (like block_size) are set.
                l1_details_dict = get_cache_level_org_details(final_config["cache_hierarchy"][0])
                l2_details_dict = None
                if len(final_config["cache_hierarchy"]) > 1:
                    l2_details_dict = get_cache_level_org_details(final_config["cache_hierarchy"][1])

                # Construct test name
                variation_tag = final_config.get("variation_tag", "UnknownVariation")
                final_config["test_name"] = f"{bench_key}_{variation_tag}"

                # Create description focusing on hit rate analysis
                desc_list = [f"Benchmark: {bench_details['description']} ({bench_key})",
                             f"Variation: {variation_tag}",
                             f"Analysis Focus: Hit rate and algorithmic behavior"]
                
                for i, level_cfg in enumerate(final_config["cache_hierarchy"]):
                    level_details = l1_details_dict if i == 0 else l2_details_dict
                    # Ensure level_cfg exists and is not None
                    if level_cfg and level_details:
                        desc_list.append(
                            f"L{level_cfg['level']}: Size={level_cfg.get('size','N/A')}B, "
                            f"Block={level_cfg.get('block_size','N/A')}B, "
                            f"Policy={level_cfg.get('policy','N/A')}, "
                            f"Org={level_details.get('label','N/A')}"
                        )
                final_config["description"] = " | ".join(desc_list)
                
                # Clean up temporary fields
                if "variation_tag" in final_config:
                    del final_config["variation_tag"]
                if "test_name_base_suffix" in final_config:
                     del final_config["test_name_base_suffix"]

                save_config_file(final_config, subdir_name, final_config["test_name"])
                count_for_set += 1
        
        print(f"  Generated {count_for_set} config files in '{os.path.join(CONFIGS_DIR, subdir_name)}'")
        total_files_generated += count_for_set

    print(f"\n--- Configuration Generation Complete ---")
    print(f"Total configuration files generated: {total_files_generated}")
    print(f"\nExperiment Focus:")
    print(f"  ✅ Hit rate analysis across cache organizations")
    print(f"  ✅ Cache capacity and conflict behavior studies") 
    print(f"  ✅ Hierarchy effectiveness vs single-level caches")
    print(f"  ✅ Inclusion policy capacity utilization analysis")
    print(f"  ✅ Memory traffic patterns (write policy impact)")
    print(f"  ❌ Timing-dependent AMAT calculations (avoided)")
    print(f"  ❌ Latency-based optimization (avoided)")

if __name__ == "__main__":
    main()
