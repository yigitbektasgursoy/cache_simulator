// binary_tree.c - Binary tree traversal with moderate locality
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_SIZE 100000  // Number of nodes

// Tree node structure
typedef struct TreeNode {
    int value;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

// Helper function to insert into BST
TreeNode* insert_tree(TreeNode* root, int value) {
    if (root == NULL) {
        TreeNode* new_node = (TreeNode*)malloc(sizeof(TreeNode));
        if (!new_node) return NULL;
        
        new_node->value = value;
        new_node->left = new_node->right = NULL;
        return new_node;
    }
    
    if (value < root->value)
        root->left = insert_tree(root->left, value);
    else
        root->right = insert_tree(root->right, value);
    
    return root;
}

// Helper function to traverse the tree recursively
void traverse_tree(TreeNode* root, volatile int* sum) {
    if (root == NULL) return;
    
    *sum += root->value;
    traverse_tree(root->left, sum);
    traverse_tree(root->right, sum);
}

// Helper function to free the tree
void free_tree(TreeNode* root) {
    if (root == NULL) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(int argc, char **argv) {
    int size = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    
    printf("Binary tree traversal - Size: %d nodes\n", size);
    
    // Create and populate binary tree
    TreeNode* root = NULL;
    int* values = (int*)malloc(size * sizeof(int));
    if (!values) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    srand(42);
    for (int i = 0; i < size; i++) {
        values[i] = rand() % 10000;
        root = insert_tree(root, values[i]);
        if (root == NULL) {
            printf("Memory allocation failed\n");
            free(values);
            return 1;
        }
    }
    
    // Traverse tree - moderate and unpredictable locality
    begin_roi();
    volatile int sum = 0;
    
    traverse_tree(root, &sum);
    
    end_roi();
    printf("Checksum: %d\n", sum);
    
    // Cleanup
    free_tree(root);
    free(values);
    
    return 0;
}
