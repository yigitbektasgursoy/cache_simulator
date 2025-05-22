// linked_list.c - Linked list traversal with poor spatial locality
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_SIZE 100000  // Number of nodes

// Node structure for linked list
typedef struct Node {
    int value;
    struct Node* next;
} Node;

void __attribute__((noinline)) begin_roi() {
    asm volatile("nop");
}

void __attribute__((noinline)) end_roi() {
    asm volatile("nop");
}

int main(int argc, char **argv) {
    int size = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    
    printf("Linked list traversal - Size: %d nodes\n", size);
    
    // Create linked list
    Node* head = NULL;
    
    srand(42);
    for (int i = 0; i < size; i++) {
        Node* new_node = (Node*)malloc(sizeof(Node));
        if (!new_node) {
            printf("Memory allocation failed\n");
            // Free already allocated nodes
            while (head != NULL) {
                Node* temp = head;
                head = head->next;
                free(temp);
            }
            return 1;
        }
        
        new_node->value = rand() % 100;
        new_node->next = head;
        head = new_node;
    }
    
    // Traverse linked list - poor spatial locality due to pointer chasing
    begin_roi();
    volatile int sum = 0;
    
    Node* current = head;
    while (current != NULL) {
        sum += current->value;  // Access memory via pointers
        current = current->next;
    }
    
    end_roi();
    printf("Checksum: %d\n", sum);
    
    // Free linked list
    while (head != NULL) {
        Node* temp = head;
        head = head->next;
        free(temp);
    }
    
    return 0;
}
