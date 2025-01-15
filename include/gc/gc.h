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


gc_holder* create_gc(pthread_t tid);


#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H
