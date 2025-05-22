// blocked_access.c - Blocked matrix traversal with good temporal locality
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_SIZE 512  // Matrix dimension
#define DEFAULT_BLOCK 64  // Block size

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int n = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    int block_size = (argc > 2) ? atoi(argv[2]) : DEFAULT_BLOCK;
    
    printf("Blocked matrix access - Matrix: %d x %d, Block: %d x %d\n", 
           n, n, block_size, block_size);
    
    // Allocate and initialize matrix
    float* matrix = (float*)malloc(n * n * sizeof(float));
    if (!matrix) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    srand(42);
    for (int i = 0; i < n*n; i++) {
        matrix[i] = (float)rand() / RAND_MAX;
    }
    
    // Blocked access - good temporal locality
    begin_roi();
    volatile float sum = 0.0f;
    
    for (int i = 0; i < n; i += block_size) {
        for (int j = 0; j < n; j += block_size) {
            // Process block
            for (int ii = i; ii < i + block_size && ii < n; ii++) {
                for (int jj = j; jj < j + block_size && jj < n; jj++) {
                    sum += matrix[ii*n + jj];  // Access by blocks
                }
            }
        }
    }
    
    end_roi();
    printf("Checksum: %f\n", sum);
    
    free(matrix);
    return 0;
}
