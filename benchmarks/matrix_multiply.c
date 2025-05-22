/**
 * Matrix Multiplication Benchmark
 * 
 * This kernel tests cache performance with different matrix multiplication algorithms:
 * 1. Naive implementation (poor cache utilization)
 * 2. Cache-friendly implementation with blocking
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define DEFAULT_SIZE 256
#define DEFAULT_BLOCK 32

// Function to mark beginning of Region of Interest (ROI)
void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");  // Special NOP that PIN tool can look for
}

// Function to mark end of Region of Interest (ROI)
void __attribute__((noinline)) end_roi() {
    asm volatile("nop");  // Special NOP that PIN tool can look for
}

// Naive matrix multiplication (poor spatial locality)
void __attribute__((noinline)) matrix_multiply_naive(float *A, float *B, float *C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int k = 0; k < n; k++) {
                sum += A[i*n + k] * B[k*n + j];
            }
            C[i*n + j] = sum;
        }
    }
}

// Cache-friendly matrix multiplication with blocking
void __attribute__((noinline)) matrix_multiply_blocked(float *A, float *B, float *C, int n, int block_size) {
    // Initialize C to zero
    memset(C, 0, n * n * sizeof(float));
    
    for (int i = 0; i < n; i += block_size) {
        for (int j = 0; j < n; j += block_size) {
            for (int k = 0; k < n; k += block_size) {
                // Process block
                for (int ii = i; ii < i + block_size && ii < n; ii++) {
                    for (int kk = k; kk < k + block_size && kk < n; kk++) {
                        for (int jj = j; jj < j + block_size && jj < n; jj++) {
                            C[ii*n + jj] += A[ii*n + kk] * B[kk*n + jj];
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    int n = DEFAULT_SIZE;
    int block_size = DEFAULT_BLOCK;
    int method = 0;  // 0 = naive, 1 = blocked
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            n = atoi(argv[i+1]);
            i++;
        } else if (strcmp(argv[i], "-b") == 0 && i+1 < argc) {
            block_size = atoi(argv[i+1]);
            i++;
        } else if (strcmp(argv[i], "-m") == 0 && i+1 < argc) {
            method = atoi(argv[i+1]);
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-n size] [-b block_size] [-m method]\n", argv[0]);
            printf("  -n size       : Matrix size (default: %d)\n", DEFAULT_SIZE);
            printf("  -b block_size : Block size for blocked method (default: %d)\n", DEFAULT_BLOCK);
            printf("  -m method     : 0 = naive, 1 = blocked (default: 0)\n");
            return 0;
        }
    }
    
    printf("Matrix size: %d x %d\n", n, n);
    if (method == 1) {
        printf("Method: Blocked (block size: %d)\n", block_size);
    } else {
        printf("Method: Naive\n");
    }
    
    // Allocate and initialize matrices
    float *A = (float*)malloc(n * n * sizeof(float));
    float *B = (float*)malloc(n * n * sizeof(float));
    float *C = (float*)malloc(n * n * sizeof(float));
    
    if (!A || !B || !C) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }
    
    // Initialize matrices with random values
    srand(time(NULL));
    for (int i = 0; i < n*n; i++) {
        A[i] = (float)rand() / RAND_MAX;
        B[i] = (float)rand() / RAND_MAX;
    }
    
    // Perform matrix multiplication
    printf("Starting computation...\n");
    
    clock_t start = clock();
    
    if (method == 1) {
        matrix_multiply_blocked(A, B, C, n, block_size);
    } else {
        matrix_multiply_naive(A, B, C, n);
    }
    
    clock_t end = clock();
    
    double seconds = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Computation time: %.2f seconds\n", seconds);
    
    // Verify result (sum of a few elements)
    float sum = 0.0f;
    for (int i = 0; i < n*n; i += n*n/10) {
        sum += C[i];
    }
    printf("Checksum: %f\n", sum);
    
    free(A);
    free(B);
    free(C);
    
    return 0;
}
