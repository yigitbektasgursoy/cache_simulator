// --- benchmarks/conflict_heavy.c ---
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_NUM_CONFLICT_ELEMENTS 3 // Number of elements intended to conflict
#define DEFAULT_L1_CACHE_SIZE_BYTES 1024
#define DEFAULT_L1_BLOCK_SIZE_BYTES 32
#define DEFAULT_L1_ASSOCIATIVITY 2
#define DEFAULT_ITERATIONS 100

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int num_conflict_elements = (argc > 1) ? atoi(argv[1]) : DEFAULT_NUM_CONFLICT_ELEMENTS;
    int l1_cache_size_bytes = (argc > 2) ? atoi(argv[2]) : DEFAULT_L1_CACHE_SIZE_BYTES;
    int l1_block_size_bytes = (argc > 3) ? atoi(argv[3]) : DEFAULT_L1_BLOCK_SIZE_BYTES;
    int l1_associativity = (argc > 4) ? atoi(argv[4]) : DEFAULT_L1_ASSOCIATIVITY;
    int iterations = (argc > 5) ? atoi(argv[5]) : DEFAULT_ITERATIONS;

    printf("Conflict-heavy - Elements: %d, L1_Size: %dB, L1_Block: %dB, L1_Assoc: %d-way, Iterations: %d\n",
           num_conflict_elements, l1_cache_size_bytes, l1_block_size_bytes, l1_associativity, iterations);

    if (num_conflict_elements <= l1_associativity) {
         printf("Warning: num_conflict_elements (%d) should be > L1 associativity (%d) to guarantee conflict misses.\n",
                num_conflict_elements, l1_associativity);
    }
    if (l1_associativity == 0) {
        printf("Error: Associativity cannot be 0.\n"); return 1;
    }


    // Calculate stride to cause conflicts.
    // Elements are in the same set if (address / block_size) % num_sets is the same.
    // num_sets = (cache_size_bytes / block_size_bytes) / associativity.
    int num_sets = (l1_cache_size_bytes / l1_block_size_bytes) / l1_associativity;
    if (num_sets == 0) {
        printf("Error: Number of sets is 0. Check cache parameters.\n"); return 1;
    }
    
    // Stride in bytes to map to the same set for different blocks.
    // Accesses at A, A + (num_sets * block_size), A + 2*(num_sets * block_size) ...
    // will map to the same set.
    int conflict_stride_bytes = num_sets * l1_block_size_bytes;
    int conflict_stride_elements = conflict_stride_bytes / sizeof(int);

    if (conflict_stride_elements == 0) {
        printf("Error: Conflict stride in elements is 0. Stride_bytes (%d) likely < sizeof(int).\n", conflict_stride_bytes);
        printf("This benchmark might not work as intended with these cache parameters for int arrays.\n");
        // It can still cause conflicts if elements are very close and block size is small,
        // but the intended striding logic for *different blocks* in the same set fails.
        // For this specific case, let's make stride at least 1 element.
        conflict_stride_elements = 1; 
        // This means it will just access adjacent elements if the calculated stride was < 1 element.
        // This isn't ideal for the "conflict across far blocks" goal but prevents malloc(0) or div by zero.
    }


    // Allocate enough memory for the strided accesses
    int total_elements_needed = conflict_stride_elements * (num_conflict_elements - 1) + 1;
    if (num_conflict_elements == 0) total_elements_needed = 0;
    if (num_conflict_elements == 1) total_elements_needed = 1;


    int* array = (int*)malloc(total_elements_needed * sizeof(int));
    if (!array && total_elements_needed > 0) {
        printf("Memory allocation failed\n");
        return 1;
    }
    // Initialize if allocated
    if (array) {
        for(int i=0; i < total_elements_needed; ++i) array[i] = i;
    }


    begin_roi();
    volatile int sum = 0;
    if (array && num_conflict_elements > 0) { // only run if array exists and elements to access
        for (int iter = 0; iter < iterations; iter++) {
            for (int i = 0; i < num_conflict_elements; i++) {
                sum += array[i * conflict_stride_elements];
            }
        }
    }
    end_roi();

    printf("Checksum: %d\n", sum);
    if (array) free(array);
    return 0;
}
