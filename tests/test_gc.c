#include "gc/gc.h"
#include "gc/minunit.h"
#include <pthread.h>
#include <unistd.h>

// Structure to test complex objects with pointers
typedef struct test_node {
    int value;
    struct test_node* next;
} test_node;

// Test basic GC initialization and shutdown
char* test_gc_init_shutdown() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    GC_STOP();
    
    // If we reached here without crashing, test passed
    MU_ASSERT(1, "GC initialization and shutdown failed");
    return NULL;
}

// Test basic memory allocation
char* test_gc_malloc() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();
    
    GC_CREATE();
    
    int* ptr = NULL;
    GC_MALLOC(ptr, sizeof(int));
    
    MU_ASSERT(ptr != NULL, "GC_MALLOC failed to allocate memory");
    
    *ptr = 42;
    MU_ASSERT(*ptr == 42, "Memory allocated by GC_MALLOC is not usable");
    
    GC_STOP();
    return NULL;
}

// Test memory allocation and explicit deallocation
char* test_gc_malloc_free() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    int* ptr = NULL;
    GC_MALLOC(ptr, sizeof(int));
    MU_ASSERT(ptr != NULL, "GC_MALLOC failed to allocate memory");
    
    int allocs_cnt_before = GC_GET_ALLOCS_CNT();

    *ptr = 100;
    
    GC_FREE(ptr);

    int allocs_cnt_after = GC_GET_ALLOCS_CNT();
    
    MU_ASSERT(allocs_cnt_before - allocs_cnt_after == 1, "GC_FREE failed");

    // Allocate again to ensure GC system still works
    int* ptr2 = NULL;
    GC_MALLOC(ptr2, sizeof(int));
    MU_ASSERT(ptr2 != NULL, "GC_MALLOC failed after GC_FREE");
    
    GC_STOP();
    return NULL;
}

// Test marking objects as roots
char* test_gc_mark_root() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    int* ptr = NULL;
    GC_MALLOC(ptr, sizeof(int));
    MU_ASSERT(ptr != NULL, "GC_MALLOC failed to allocate memory");
    
    int roots_before = GC_GET_ROOTS_CNT();
    GC_MARK_ROOT(ptr);
    int roots_after = GC_GET_ROOTS_CNT();

    // Test that root was actually marked
    MU_ASSERT(roots_after - roots_before == 1, "Mark root failed");
    
    // Collect garbage - should not collect marked roots
    GC_COLLECT(THREAD_LOCAL);
    
    // Verify ptr is still valid
    *ptr = 200;
    MU_ASSERT(*ptr == 200, "Root object was incorrectly collected by GC");
    
    GC_FREE(ptr);
    
    GC_STOP();
    return NULL;
}


char* test_gc_unmark_root() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    int* ptr = NULL;
    GC_MALLOC(ptr, sizeof(int));
    MU_ASSERT(ptr != NULL, "GC_MALLOC failed to allocate memory");

    // Number of allocs before garabge collection
    int allocs_cnt_before = GC_GET_ALLOCS_CNT();

    int roots_before = GC_GET_ROOTS_CNT();
    GC_MARK_ROOT(ptr);
    int roots_after = GC_GET_ROOTS_CNT();

    // Check that root was marked
    MU_ASSERT(roots_after - roots_before == 1, "Mark root failed");

    roots_before = GC_GET_ROOTS_CNT();
    GC_UNMARK_ROOT(ptr);
    roots_after = GC_GET_ROOTS_CNT();

    // Check that root was actually unmarked 
    MU_ASSERT(roots_before - roots_after == 1, "Unmark root failed");
    
    GC_FREE(ptr);
    GC_COLLECT(THREAD_LOCAL);

    int allocs_cnt_after = GC_GET_ALLOCS_CNT();
    // Check that garabge from unmarked root was collected
    MU_ASSERT(allocs_cnt_before - allocs_cnt_after == 1, "Unmarked root sweepin failed");
    
    GC_STOP();
    return NULL;
}

// Test garbage collection with complex object graph
char* test_gc_complex_objects() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    // Create a linked list
    test_node* head = NULL;
    GC_MALLOC(head, sizeof(test_node));
    MU_ASSERT(head != NULL, "Failed to allocate head node");
    
    head->value = 1;
    head->next = NULL;
    
    // Add more nodes
    test_node* current = head;
    for (int i = 2; i <= 5; i++) {
        test_node* new_node = NULL;
        GC_MALLOC(new_node, sizeof(test_node));
        MU_ASSERT(new_node != NULL, "Failed to allocate node");
        
        new_node->value = i;
        new_node->next = NULL;
        current->next = new_node;
        current = new_node;
    }
    
    // Mark only the head as root
    GC_MARK_ROOT(head);
    
    // Collect garbage with THREAD_LOCAL flag
    GC_COLLECT(THREAD_LOCAL);
    
    // Verify the linked list is still intact
    current = head;
    for (int i = 1; i <= 5; i++) {
        MU_ASSERT(current != NULL, "Node was incorrectly collected by GC");
        MU_ASSERT(current->value == i, "Node value was corrupted");
        current = current->next;
    }
    
    GC_STOP();
    return NULL;
}

// Test thread-local garbage collection
char* test_gc_thread_local_collection() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    // Allocate several objects
    int* ptr1 = NULL;
    GC_MALLOC(ptr1, sizeof(int));
    
    int* ptr2 = NULL;
    GC_MALLOC(ptr2, sizeof(int));

    int allocs_before = GC_GET_ALLOCS_CNT();
    
    // Mark one as root
    GC_MARK_ROOT(ptr1);
    
    ptr1 = NULL;
    *ptr2 = 222;
    
    // Run garbage collection with THREAD_LOCAL flag
    GC_COLLECT(THREAD_LOCAL);

    int allocs_after = GC_GET_ALLOCS_CNT();
    
    // Check that losted and not marked as root allocations were collectes 
    MU_ASSERT(allocs_before - allocs_after == 2, "Thread-local garbage collection failed");
    
    GC_STOP();
    return NULL;
}

// Function for worker threads
void* thread_func(void* arg) {
    gc_handler handler = gc_create(pthread_self());
    int* val;
    handler.mark_root(pthread_self(), &val);
    
    for (size_t i = 0; i < 3; i++)
    {
        handler.gc_malloc(pthread_self(), (void**)&val, 4);
        *val = 123;
        usleep(200000);     // 200ms
    }
    
    gc_stop(pthread_self());
    return NULL;
}

// Test multi-threaded GC
char* test_gc_global_collection() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    const int num_thread = 9;
    pthread_t threads[num_thread];

    for (size_t i = 0; i < num_thread; i++)
    {
        if (pthread_create(&threads[i], NULL, thread_func, NULL) != 0)
        {
            perror("pthread_create failed");
            return NULL;
        }
    }


    gc_handler handler = gc_create(pthread_self());
    usleep(500000);     // 200ms

    int allocs_before = gc_gel_all_threads_allocs_cnt();

    // Run global garbage collection
    handler.collect(pthread_self(), GLOBAL);

    int allocs_after = gc_gel_all_threads_allocs_cnt();

    // Check that 18 allocations were collected by gc
    MU_ASSERT(allocs_before - allocs_after == 18, "Multi-threaded gc failed");

    GC_STOP();

    // Wait for all threads to complete
    for (size_t i = 0; i < num_thread; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            perror("pthread_join failed");
            return NULL;
        }   
    }
    
    return NULL;
}

// Backgroung collection test
char* test_gc_background_collection() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();

    // Allocate a large block of memory
    char* large_block = NULL;
    size_t large_size = 1024 * 1024; // 1MB
    GC_MALLOC(large_block, large_size);

    /*
        Now the amount of allocated memory has
        reached the garbage sweep factor (initally 1024), 
        so the next allocation will start 
        garbage collection.
    */

    int allocs_before = GC_GET_ALLOCS_CNT();
    GC_MALLOC(large_block, large_size);
    int allocs_after = GC_GET_ALLOCS_CNT();

    MU_ASSERT(allocs_before == allocs_after, "Background collection failed");

    GC_STOP();
    return NULL;
}

// Large allocation test
char* test_gc_large_allocation() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    // Allocate a large block of memory
    char* large_block = NULL;
    size_t large_size = 1024 * 1024; // 1MB
    GC_MALLOC(large_block, large_size);
    
    MU_ASSERT(large_block != NULL, "Failed to allocate large memory block");
    
    // Write to memory to ensure it's usable
    memset(large_block, 'A', large_size);
    
    // Verify some bytes
    MU_ASSERT(large_block[0] == 'A', "Memory content corrupted");
    MU_ASSERT(large_block[large_size/2] == 'A', "Memory content corrupted");
    MU_ASSERT(large_block[large_size-1] == 'A', "Memory content corrupted");
    
    GC_STOP();
    return NULL;
}

// Stress test with many allocations
char* test_gc_stress() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    GC_CREATE();
    
    #define STRESS_COUNT 1000
    void* pointers[STRESS_COUNT];
    
    // Allocate many small objects and mark them as roots
    for (int i = 0; i < STRESS_COUNT; i++) {
        int* ptr = NULL;
        GC_MALLOC(ptr, sizeof(int));
        MU_ASSERT(ptr != NULL, "Stress test allocation failed");
        *ptr = i;
        pointers[i] = ptr;
        GC_MARK_ROOT(pointers[i]);
    }
    
    // Run garbage collection
    GC_COLLECT(THREAD_LOCAL);
    
    // Verify roots are still accessible
    for (int i = 0; i < STRESS_COUNT; ++i) {
        int* ptr = (int*)pointers[i];
        MU_ASSERT(*ptr == i, "Root value corrupted in stress test");
    }
    
    GC_STOP();
    return NULL;
}

// Test passing invalid argument to gc_handler
char* test_gc_passing_inval() {
    // Stopping to make sure a new garbage collector is going to be created
    GC_STOP();

    gc_handler handler = GC_CREATE();

    int* val;
    errno = 0;
    handler.gc_malloc(pthread_self() + 1, (void**)&val, 4);
    MU_ASSERT(errno == EINVAL, "Test with passign invalid argument to gc_malloc() failed");

    errno = 0;
    handler.gc_free(pthread_self() + 1, &val);
    MU_ASSERT(errno == EINVAL, "Test with passign invalid argument to gc_free() failed");

    errno = 0;
    handler.mark_root(pthread_self() + 1, &val);
    MU_ASSERT(errno == EINVAL, "Test with passign invalid argument to mark_root() failed");

    errno = 0;
    handler.unmark_root(pthread_self() + 1, &val);
    MU_ASSERT(errno == EINVAL, "Test with passign invalid argument to unmark_root() failed");

    errno = 0;
    handler.collect(pthread_self() + 1, THREAD_LOCAL);
    MU_ASSERT(errno == EINVAL, "Test with passign invalid id argument to collect() failed");

    errno = 0;
    handler.collect(pthread_self(), 4);
    MU_ASSERT(errno == EINVAL, "Test with passign invalid flag argument to collect() failed");
    
    GC_STOP();
    return NULL;
}

int tests_run = 0;

static char* basic_functionality_test_suite() {
    // Basic functionality tests
    MU_RUN_TEST(test_gc_init_shutdown);
    MU_RUN_TEST(test_gc_malloc);
    MU_RUN_TEST(test_gc_malloc_free);
    MU_RUN_TEST(test_gc_mark_root);
    MU_RUN_TEST(test_gc_complex_objects);

    return NULL;
}

static char* collection_test_suite() {
    // Collection tests
    MU_RUN_TEST(test_gc_unmark_root);
    MU_RUN_TEST(test_gc_thread_local_collection);
    MU_RUN_TEST(test_gc_global_collection);
    MU_RUN_TEST(test_gc_background_collection);

    return NULL;
}

static char* advanced_test_suite() {
    // Advanced tests
    MU_RUN_TEST(test_gc_large_allocation);
    MU_RUN_TEST(test_gc_stress);

    return NULL;
}

static char* error_handling_test_suite() {
    // Error handling tests
    MU_RUN_TEST(test_gc_passing_inval);

    return NULL;
}

int run_test_suite(const char* name, char*(*test_suite)()) {
    printf("%s: ", name);
    char *result = test_suite();
    if (result) {
        printf("FAILED\n");
        printf("\t%s\n", result);
        return 0;
    }

    printf("OK\n");
    return 1;
}

int main() {
    printf("=====[ GC tests ]=====\n");

    int isAllPassed = 1;
    isAllPassed &= run_test_suite("Basic functionality", basic_functionality_test_suite);
    isAllPassed &= run_test_suite("Collection", collection_test_suite);
    isAllPassed &= run_test_suite("Advanced", advanced_test_suite);
    isAllPassed &= run_test_suite("Error handling", error_handling_test_suite);

    printf("======================\n");
    if (isAllPassed)
    {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);
    printf("======================\n");
    
    return !isAllPassed;
}