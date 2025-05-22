// --- benchmarks/random_access.c ---
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Default: 4096 elements (16KB for ints), 2000 random accesses
#define DEFAULT_ARRAY_SIZE 4096
#define DEFAULT_NUM_ACCESSES 2000

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int array_size = (argc > 1) ? atoi(argv[1]) : DEFAULT_ARRAY_SIZE;
    int num_accesses = (argc > 2) ? atoi(argv[2]) : DEFAULT_NUM_ACCESSES;

    printf("Random access - Array Size: %d elements, Accesses: %d\n", array_size, num_accesses);

    int* array = (int*)malloc(array_size * sizeof(int));
    if (!array) {
        printf("Memory allocation failed for array\n");
        return 1;
    }

    srand(42); // Consistent randomness
    for (int i = 0; i < array_size; i++) {
        array[i] = rand() % 100;
    }

    // Generate indices beforehand to avoid rand() in ROI if it causes issues with Pin
    int* access_indices = (int*)malloc(num_accesses * sizeof(int));
    if(!access_indices) {
        printf("Memory allocation failed for indices\n");
        free(array);
        return 1;
    }
    for (int i = 0; i < num_accesses; i++) {
        access_indices[i] = rand() % array_size;
    }

    begin_roi();
    volatile int sum = 0;
    for (int i = 0; i < num_accesses; i++) {
        sum += array[access_indices[i]];
    }
    end_roi();

    printf("Checksum: %d\n", sum);
    free(array);
    free(access_indices);
    return 0;
}
