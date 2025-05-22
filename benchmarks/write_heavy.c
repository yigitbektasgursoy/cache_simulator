// --- benchmarks/write_heavy.c ---
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_SIZE 64      // Small array, 64*4 = 256 bytes
#define DEFAULT_ITERATIONS 100 // Many passes

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int size = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    int iterations = (argc > 2) ? atoi(argv[2]) : DEFAULT_ITERATIONS;

    printf("Write-heavy - Size: %d elements, Iterations: %d\n", size, iterations);

    int* array = (int*)malloc(size * sizeof(int));
    if (!array) {
        printf("Memory allocation failed\n");
        return 1;
    }

    // Initialize
    for (int i = 0; i < size; i++) {
        array[i] = i;
    }

    begin_roi();
    volatile int temp = 0; // To ensure reads are not optimized out and add some read traffic
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < size; i++) {
            array[i] = (array[i] + iter + i + temp) % 10000; // Write, keep values bounded
        }
        // Occasional read to mix in some read traffic and use temp
        if (iter % 5 == 0) {
             for (int i = 0; i < size; i = i + (size/4 > 0 ? size/4 : 1) ) { // Read a few elements
                temp = (temp + array[i]) % 10000;
             }
        }
    }
    end_roi();

    long long sum = 0;
    for(int i=0; i<size; ++i) sum += array[i];
    printf("Checksum: %lld (final temp: %d)\n", sum, temp);

    free(array);
    return 0;
}
