#include <iostream>
#include <unordered_map>
#include "gc/gc.h"

/**
 * @struct Allocation
 * @brief Represents a memory allocation with additional metadata for management.
 */
struct Allocation
{
    /**
     * @brief Pointer to the allocated memory block.
     */
    void* ptr;

    /**
     * @brief Pointer to a destructor function for the memory block.
     * 
     * The destructor is called to properly clean up the memory when it is no longer needed.
     * 
     * @param void* A pointer to the memory block to be destroyed.
     */
    void(*dtor)(void*);

    /**
     * @brief Tag used during the mark phase of garbage collection or memory management.
     * 
     * This tag helps to identify the state of the memory block.
     */
    uint8_t tag;

    /**
     * @brief Size of the allocated memory block, in bytes.
     */
    size_t size;
};

struct garbage_collector{
    pid_t tid;

    garbage_collector(pid_t tid) : tid(tid) {};

    void* malloc(size_t size) {
        return NULL;
    }

    void free(void* ptr) {

    }
};

void* gc_malloc_wrapper(void* obj, size_t size) {
    return static_cast<garbage_collector*>(obj)->malloc(size);
}

void gc_free_wrapper(void* obj, void* ptr) {
    static_cast<garbage_collector*>(obj)->free(ptr);
}

static std::unordered_map<pid_t, garbage_collector*> gc_register;

gc_holder* create_gc(pid_t tid) {
    gc_register[tid] = new garbage_collector(tid);
    gc_holder* holder =  new gc_holder();
    
    holder->malloc = &gc_malloc_wrapper;
    holder->free = &gc_free_wrapper;
    holder->context = gc_register[tid];

    return holder;
}

