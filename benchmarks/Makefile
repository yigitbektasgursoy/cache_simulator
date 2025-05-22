# Compiler and flags
CC = gcc
# Use -std=gnu99 if your C files use 'asm'. Use -std=c99 if they use '__asm__'.
CFLAGS = -O0 -Wall -Wextra -std=gnu99

# --- Path Definitions ---
# BENCHMARKS_DIR is the current directory where this Makefile resides
BENCHMARKS_DIR := $(CURDIR)
# PROJECT_ROOT is one level up from the benchmarks directory
PROJECT_ROOT := $(realpath $(BENCHMARKS_DIR)/..)

# PIN_SCRIPT is now in $(PROJECT_ROOT)/scripts/
# Ensure PIN_ROOT environment variable is set, or define it here:
# PIN_ROOT_DEFAULT = /path/to/your/pin/installation
# PIN_ROOT ?= $(PIN_ROOT_DEFAULT) 
# ^ The above is if run_pin_trace.sh doesn't hardcode PIN_ROOT
PIN_SCRIPT = $(PROJECT_ROOT)/scripts/run_pin_trace.sh

# TRACES_OUT_DIR is $(PROJECT_ROOT)/traces
TRACES_OUT_DIR = $(PROJECT_ROOT)/traces

# Target executables
TARGETS = sequential_access strided_access random_access row_major column_major \
          blocked_access linked_list binary_tree matrix_multiply \
          write_heavy conflict_heavy

# Default target
all: $(TARGETS)

# Individual target compilation rules
sequential_access: sequential_access.c
	$(CC) $(CFLAGS) -o $@ $<
strided_access: strided_access.c
	$(CC) $(CFLAGS) -o $@ $<
random_access: random_access.c
	$(CC) $(CFLAGS) -o $@ $<
row_major: row_major.c
	$(CC) $(CFLAGS) -o $@ $<
column_major: column_major.c
	$(CC) $(CFLAGS) -o $@ $<
blocked_access: blocked_access.c
	$(CC) $(CFLAGS) -o $@ $<
linked_list: linked_list.c
	$(CC) $(CFLAGS) -o $@ $<
binary_tree: binary_tree.c
	$(CC) $(CFLAGS) -o $@ $<
matrix_multiply: matrix_multiply.c
	$(CC) $(CFLAGS) -o $@ $<
write_heavy: write_heavy.c
	$(CC) $(CFLAGS) -o $@ $<
conflict_heavy: conflict_heavy.c
	$(CC) $(CFLAGS) -o $@ $<

# Clean up executables in this directory and traces in the project root's trace dir
clean:
	@echo "Cleaning executables in $(BENCHMARKS_DIR)..."
	rm -f $(TARGETS) *.o
	@echo "Cleaning traces in $(TRACES_OUT_DIR)..."
	rm -rf $(TRACES_OUT_DIR)
	@echo "Cleaning configurations in $(PROJECT_ROOT)/configs (if they exist)..."
	rm -rf $(PROJECT_ROOT)/configs 
	# ^ Be careful with this if configs are manually curated. 
	#   It's fine if they are always regenerated.

# Rule to create the global traces directory
$(TRACES_OUT_DIR):
	@echo "Ensuring trace directory exists: $(TRACES_OUT_DIR)"
	mkdir -p $(TRACES_OUT_DIR)

# Generate traces
traces: all $(TRACES_OUT_DIR) # Depends on executables and the trace directory existing
	@echo "Generating traces into $(TRACES_OUT_DIR)..."
	@echo "Using PIN script: $(PIN_SCRIPT)"
	@if [ ! -f "$(PIN_SCRIPT)" ]; then \
		echo "Error: PIN script $(PIN_SCRIPT) not found."; \
		exit 1; \
	elif [ ! -x "$(PIN_SCRIPT)" ]; then \
		echo "Error: PIN script $(PIN_SCRIPT) is not executable. Please use 'chmod +x $(PIN_SCRIPT)'."; \
		exit 1; \
	fi
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/sequential_access.out -- ./sequential_access 1000 1
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/strided_access.out -- ./strided_access 500 1 64
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/random_access.out -- ./random_access 4096 2000
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/row_major.out -- ./row_major 16
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/column_major.out -- ./column_major 16
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/blocked_access.out -- ./blocked_access 24 8
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/linked_list.out -- ./linked_list 1000
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/binary_tree.out -- ./binary_tree 1000
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/matrix_naive.out -- ./matrix_multiply -n 16 -m 0
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/matrix_blocked.out -- ./matrix_multiply -n 16 -m 1 -b 4
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/write_heavy.out -- ./write_heavy 64 100
	$(PIN_SCRIPT) -o $(TRACES_OUT_DIR)/conflict_heavy.out -- ./conflict_heavy 3 1024 32 2 100
	@echo "Trace generation complete."

# Rule to run the config generator script (which is in $(PROJECT_ROOT)/scripts/)
configs:
	@echo "Generating configurations using script: $(PROJECT_ROOT)/scripts/generate_configs.py"
	@if [ ! -f "$(PROJECT_ROOT)/scripts/generate_configs.py" ]; then \
		echo "Error: Config generation script not found at $(PROJECT_ROOT)/scripts/generate_configs.py"; \
		exit 1; \
	fi
	python3 $(PROJECT_ROOT)/scripts/generate_configs.py

# Perform a complete setup: clean, compile benchmarks, generate traces, generate configs
setup: clean all traces configs
	@echo "Setup complete. Executables, traces, and configurations are ready."
	@echo "Traces are in: $(TRACES_OUT_DIR)"
	@echo "Configurations are in: $(PROJECT_ROOT)/configs"


# Test rule (runs executables directly, doesn't use traces or configs)
test: all
	@echo "Running quick tests of benchmark executables..."
	./sequential_access 1000 1
	./strided_access 500 1 64
	./random_access 1024 500
	./row_major 16
	./column_major 16
	./blocked_access 24 8
	./linked_list 1000
	./binary_tree 1000
	./matrix_multiply -n 16 -m 0
	./matrix_multiply -n 16 -m 1 -b 4
	./write_heavy 64 10
	./conflict_heavy 3 1024 32 2 10
	@echo "Quick tests complete."

.PHONY: all clean test traces configs setup
