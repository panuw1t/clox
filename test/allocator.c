#include <assert.h>

#include "memory.h"
#include "allocator.h"
#include <string.h>

// --- The Tests ---

void test_basic_allocation() {
    printf("Running basic allocation test...\n");

    // Request 10 bytes, it should align to 16 (or nearest multiple of 8)
    void* ptr1 = my_malloc(10);
    assert(ptr1 != NULL);

    // Write data to ensure it doesn't segfault
    strcpy((char*)ptr1, "Hello!");
    assert(strcmp((char*)ptr1, "Hello!") == 0);

    void* ptr2 = my_malloc(20);
    assert(ptr2 != NULL);
    assert(ptr1 != ptr2); // Must be distinct memory addresses

    printf("  -> Passed basic allocation.\n");
}

void test_free_and_reuse() {
    printf("Running free and reuse test...\n");

    void* ptr1 = my_malloc(32);
    void* ptr2 = my_malloc(32);

    // Free the first block
    my_free(ptr1);

    // Allocate a new block of the exact same size.
    // A good First-Fit allocator should reuse ptr1's old spot.
    void* ptr3 = my_malloc(32);
    assert(ptr1 == ptr3);

    printf("  -> Passed free and reuse.\n");
}

void test_coalescing() {
    printf("Running coalescing (defragmentation) test...\n");

    void* a = my_malloc(64);
    void* b = my_malloc(64);
    void* c = my_malloc(64);

    // Free 'a' and 'b'. They are adjacent, so they should merge.
    my_free(a);
    my_free(b);

    // Now ask for 100 bytes.
    // If they merged, this will succeed and use the merged space.
    // If they didn't merge, this might fail or pull from further down the heap.
    void* large_ptr = my_malloc(100);
    assert(large_ptr != NULL);
    assert(large_ptr == a); // Should reuse the merged spot

    printf("  -> Passed coalescing.\n");
}

void test_reallocate() {
    printf("Running reallocate edge cases test...\n");

    // 1. Realloc with NULL should act like malloc
    void* ptr = my_reallocate(NULL, 0, 16);
    assert(ptr != NULL);
    strcpy((char*)ptr, "Testing");

    // 2. Realloc to grow
    void* grown_ptr = my_reallocate(ptr, 16, 32);
    assert(grown_ptr != NULL);
    assert(strcmp((char*)grown_ptr, "Testing") == 0); // Data must survive

    // 3. Realloc to 0 should act like free
    void* freed_ptr = my_reallocate(grown_ptr, 32, 0);
    assert(freed_ptr == NULL); // Should return NULL when freeing

    printf("  -> Passed reallocate.\n");
}

void test_out_of_memory() {
    printf("Running out of memory test...\n");
    // Ask for a massive amount of memory that exceeds the remaining heap capacity
    void* impossible = my_malloc(9999999);
    assert(impossible == NULL); // Allocator must gracefully return NULL

    printf("  -> Passed out of memory gracefully.\n");
}

void test() {
    printf("=== Starting Custom Heap Test Suite ===\n\n");

    // Initialize a 4KB heap for testing
    init_my_heap(4096);

    test_basic_allocation();
    test_free_and_reuse();
    test_coalescing();
    test_reallocate();
    test_out_of_memory();

    printf("\n=== All tests passed successfully! ===\n");
}
