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
base_config_template = {
    # "test_name_base" will be used as a suffix after BenchmarkKey_VariationType_
    # It's effectively a placeholder if the variation function doesn't provide a more specific name part.
    "test_name_base_suffix": "DefaultL1L2",
    "cache_hierarchy": [
        {
            "level": 1, "organization": "SetAssociative", "size": 1024,
            "block_size": 32, "associativity": 2, "policy": "LRU",
            "access_latency": 1, # This will be overridden by CACTI if integrated
            "write_back": True, "write_allocate": True
        },
        {
            "level": 2, "organization": "SetAssociative", "size": 8192,
            "block_size": 32, "associativity": 4, "policy": "LRU",
            "access_latency": 10, # This will be overridden by CACTI if integrated
            "write_back": True, "write_allocate": True,
            "inclusion_policy": "Inclusive"
        }
    ],
    "memory": {"access_latency": 100},
    "trace": {"type": "File", "filename": ""}, # To be filled per benchmark
    "description": "" # To be filled per config
}

# --- BENCHMARKS: Using more descriptive keys and names ---
BENCHMARKS = {
    "SequentialAccess": {"trace_file": "sequential_access.out", "description": "Sequential memory access pattern"},
    "StridedAccess": {"trace_file": "strided_access.out", "description": "Strided memory access pattern"},
    "RowMajorMatrix": {"trace_file": "row_major.out", "description": "Row-major matrix traversal"},
    "ColumnMajorMatrix": {"trace_file": "column_major.out", "description": "Column-major matrix traversal"},
    "RandomAccess": {"trace_file": "random_access.out", "description": "Random memory access pattern"},
    "LinkedListTraversal": {"trace_file": "linked_list.out", "description": "Linked list traversal"},
    "BlockedDataAccess": {"trace_file": "blocked_access.out", "description": "Blocked data access pattern"},
    "WriteHeavy": {"trace_file": "write_heavy.out", "description": "Write-intensive workload"},
    # Add your other benchmarks here
    "MatrixNaive": {"trace_file": "matrix_naive.out", "description": "Naive matrix multiplication"},
    "MatrixBlocked": {"trace_file": "matrix_blocked.out", "description": "Blocked matrix multiplication"},
    "BinaryTree": {"trace_file": "binary_tree.out", "description": "Binary tree traversal"},
    "ConflictHeavy": {"trace_file": "conflict_heavy.out", "description": "Conflict-heavy access pattern"},

}
BENCHMARK_GROUPS = {
    "all": list(BENCHMARKS.keys()),
    "quick_test": ["SequentialAccess", "RandomAccess", "LinkedListTraversal"] # For faster testing
}

def get_num_blocks(size_bytes, block_size_bytes):
    if not isinstance(size_bytes, int) or not isinstance(block_size_bytes, int): return 0
    if block_size_bytes <= 0: return 0
    return size_bytes // block_size_bytes

def get_cache_level_org_details(cache_level_config):
    """
    Updates 'organization' and 'associativity' (caps it) in cache_level_config.
    Returns a dictionary: {'label': str, 'org_string': str, 'eff_assoc': int}
    'label' is for filenames/plotting (e.g., "FullyAssoc", "4way").
    """
    size_val = cache_level_config.get("size", 0)
    block_size_val = cache_level_config.get("block_size", 0)
    # Use a default associativity if not present, though it should be
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

    actual_assoc = min(assoc_val, num_blocks) # Cap associativity
    cache_level_config["associativity"] = actual_assoc # Update in the config dict

    if actual_assoc >= num_blocks:
        details.update({'label': "FullyAssoc", 'org_string': "FullyAssociative", 'eff_assoc': actual_assoc})
    elif actual_assoc == 1:
        details.update({'label': "DirectMapped", 'org_string': "DirectMapped", 'eff_assoc': actual_assoc})
    else:
        details.update({'label': f"{actual_assoc}way", 'org_string': "SetAssociative", 'eff_assoc': actual_assoc})

    cache_level_config["organization"] = details['org_string']
    return details


# --- Parameter Variation Functions ---
# Each returns a list of config objects.
# Each config object should have a "variation_tag" field describing the change.

def generate_base_configs():
    """Generates the base configuration without specific variations applied by these functions."""
    config = copy.deepcopy(base_config_template)
    config["variation_tag"] = "BaseConfig" # Default variation tag
    return [config]

def vary_l1_cache_size():
    configs = []
    l1_sizes = [512, 1024, 2048, 4096, 8192]
    for size_val in l1_sizes:
        config = copy.deepcopy(base_config_template)
        size_kb_str = f"{size_val//1024}KB" if size_val >= 1024 else f"{size_val}B"
        config["cache_hierarchy"][0]["size"] = size_val
        config["variation_tag"] = f"L1Size_{size_kb_str}"
        configs.append(config)
    return configs

def vary_l1_block_size():
    configs = []
    l1_block_sizes = [16, 32, 64, 128]
    for bs_val in l1_block_sizes:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["block_size"] = bs_val
        if len(config["cache_hierarchy"]) > 1: # Adjust L2 block size if L1 changes
            config["cache_hierarchy"][1]["block_size"] = max(bs_val, config["cache_hierarchy"][1].get("block_size", bs_val))
        config["variation_tag"] = f"L1Block_{bs_val}B"
        configs.append(config)
    return configs

def vary_l1_associativity():
    configs = []
    assoc_targets = [1, 2, 4, 8, 16, 32] # Target associativities for L1
    base_l1_size = base_config_template["cache_hierarchy"][0]["size"]
    base_l1_block_size = base_config_template["cache_hierarchy"][0]["block_size"]
    max_assoc_for_base_l1 = get_num_blocks(base_l1_size, base_l1_block_size)
    if max_assoc_for_base_l1 > 0: assoc_targets.append(max_assoc_for_base_l1)
    unique_assoc_targets = sorted(list(set(assoc_targets)))

    for target_assoc in unique_assoc_targets:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["associativity"] = target_assoc
        # The label for variation_tag will be determined by get_cache_level_org_details
        # when the config is finalized. Here we just set the target.
        # We'll let the main loop construct the final variation tag based on the *effective* org.
        # For now, we can store the target for more explicit naming.
        temp_l1_details = get_cache_level_org_details(config["cache_hierarchy"][0]) # Get the label
        config["variation_tag"] = f"L1Assoc_{temp_l1_details['label']}"
        configs.append(config)
    return configs

def vary_l1_replacement_policy():
    configs = []
    policies = ["LRU", "FIFO", "RANDOM"]
    for policy_val in policies:
        config = copy.deepcopy(base_config_template)
        config["cache_hierarchy"][0]["policy"] = policy_val
        config["variation_tag"] = f"L1Policy_{policy_val}"
        configs.append(config)
    return configs

# Add more variation functions (L1 write policy, L1-only, L2 variations) here if needed.

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
        print("\nERROR: One or more trace files are missing. Please ensure they exist in the TRACES_DIR.")
    return all_exist

def main():
    print(f"--- Configuration Generator ---")
    print(f"Project Root: {PROJECT_ROOT}")
    print(f"Config Output Directory: {CONFIGS_DIR}")
    print(f"Trace Directory: {TRACES_DIR}\n")

    if not check_trace_files_exist():
        return

    # Define sets of variations to generate
    # The key is the subdirectory name, value is the function generating those configs
    variation_sets = {
        "00_Base_Configs": generate_base_configs,
        "01_L1_Size_Variations": vary_l1_cache_size,
        "02_L1_BlockSize_Variations": vary_l1_block_size,
        "03_L1_Associativity_Variations": vary_l1_associativity,
        "04_L1_Policy_Variations": vary_l1_replacement_policy,
        # Add other variation set function calls here
    }

    benchmarks_to_run_against = BENCHMARK_GROUPS["all"] # Or "quick_test"
    total_files_generated = 0

    for subdir_name, generator_func in variation_sets.items():
        print(f"\nGenerating for parameter set: {subdir_name}")
        param_varied_configs = generator_func() # Get list of base configs with one param varied
        count_for_set = 0

        for base_varied_config in param_varied_configs:
            for bench_key in benchmarks_to_run_against:
                bench_details = BENCHMARKS[bench_key]
                
                # Create a fresh copy for this specific benchmark + variation
                final_config = copy.deepcopy(base_varied_config)

                # 1. Set Trace File
                final_config["trace"]["filename"] = os.path.join(TRACES_DIR, bench_details["trace_file"])

                # 2. Finalize organization and effective associativity for all cache levels
                # This modifies final_config["cache_hierarchy"] in place
                l1_details_dict = get_cache_level_org_details(final_config["cache_hierarchy"][0])
                l2_details_dict = None
                if len(final_config["cache_hierarchy"]) > 1:
                    l2_details_dict = get_cache_level_org_details(final_config["cache_hierarchy"][1])

                # 3. Construct the final test_name for JSON and filename
                # The variation_tag should already be set by the variation function
                variation_tag = final_config.get("variation_tag", "UnknownVariation")
                final_config["test_name"] = f"{bench_key}_{variation_tag}" # e.g., SequentialAccess_L1Assoc_FullyAssoc

                # 4. Create a comprehensive description
                desc_list = [f"Benchmark: {bench_details['description']} ({bench_key})",
                             f"Variation: {variation_tag}"]
                for i, level_cfg in enumerate(final_config["cache_hierarchy"]):
                    level_details = l1_details_dict if i == 0 else l2_details_dict
                    if level_details:
                        desc_list.append(
                            f"L{level_cfg['level']}: Size={level_cfg['size']}B, "
                            f"Block={level_cfg['block_size']}B, "
                            f"Policy={level_cfg['policy']}, "
                            f"Org={level_cfg['organization']} (Effective: {level_details['label']}, {level_details['eff_assoc']} ways)"
                        )
                final_config["description"] = " | ".join(desc_list)
                
                # Remove temporary tag if it was added by variation function and not part of final schema
                if "variation_tag" in final_config:
                    del final_config["variation_tag"]
                if "test_name_base_suffix" in final_config: # Clean up base template key
                     del final_config["test_name_base_suffix"]


                save_config_file(final_config, subdir_name, final_config["test_name"])
                count_for_set += 1
        
        print(f"  Generated {count_for_set} config files in '{os.path.join(CONFIGS_DIR, subdir_name)}'")
        total_files_generated += count_for_set

    print(f"\n--- Configuration Generation Complete ---")
    print(f"Total configuration files generated: {total_files_generated}")

if __name__ == "__main__":
    main()
