#ifndef GC_PROJECT_GC_H
#define GC_PROJECT_GC_H
#include <sys/types.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GET_TID pthread_self() 

void gc_create(pthread_t tid, void* bos);

void* gc_malloc(pthread_t tid, size_t size);

void gc_free(pthread_t tid, void* ptr);

void gc_run(pthread_t tid);

void* thread_gc_run(void* arg);

#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H
