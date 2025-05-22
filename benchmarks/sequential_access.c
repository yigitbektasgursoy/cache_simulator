// sequential_access.c - Sequential array access with good spatial locality
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_SIZE (1024 * 1024)  // 1M elements
#define DEFAULT_ITERATIONS 3

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int size = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    int iterations = (argc > 2) ? atoi(argv[2]) : DEFAULT_ITERATIONS;
    
    printf("Sequential access - Size: %d elements, Iterations: %d\n", size, iterations);
    
    // Allocate and initialize array
    int* array = (int*)malloc(size * sizeof(int));
    if (!array) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    srand(42);
    for (int i = 0; i < size; i++) {
        array[i] = rand() % 100;
    }
    
    // Sequential access - excellent spatial locality
    begin_roi();
    volatile int sum = 0;
    
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < size; i++) {
            sum += array[i];  // Access consecutive memory locations
        }
    }
    
    end_roi();
    printf("Checksum: %d\n", sum);
    
    free(array);
    return 0;
}
