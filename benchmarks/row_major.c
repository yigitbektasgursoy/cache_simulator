// row_major.c - Row-major matrix traversal with good spatial locality
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_SIZE 512  // Matrix dimension

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int n = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    
    printf("Row-major matrix access - Size: %d x %d\n", n, n);
    
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
    
    // Row-major access - good spatial locality
    begin_roi();
    volatile float sum = 0.0f;
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            sum += matrix[i*n + j];  // Access row by row (consecutive memory)
        }
    }
    
    end_roi();
    printf("Checksum: %f\n", sum);
    
    free(matrix);
    return 0;
}
