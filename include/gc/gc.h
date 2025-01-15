#ifndef GC_PROJECT_GC_H
#define GC_PROJECT_GC_H
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef struct gc_holder {
    void*(*malloc)(void* obj, size_t);
    void(*free)(void* obj, void* ptr);
    void* context;
} gc_holder; 


gc_holder* create_gc(pid_t tid);


/**
 * @brief Allocates a memory block of the specified size with garbage collection support.
 * 
 * This function allocates a block of memory and registers it for management
 * by the garbage collector. The allocated memory is uninitialized.
 * 
 * @param size The size of the memory block to allocate, in bytes.
 * @return A pointer to the allocated memory block, or `nullptr` if allocation fails.
 * 
 * @note The allocated memory should not be manually freed; it will be managed
 *       by the garbage collector.
 */
void* gc_malloc(size_t size);



void gc_free(void* ptr);


#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H
