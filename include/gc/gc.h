#ifndef GC_PROJECT_GC_H
#define GC_PROJECT_GC_H
#include <sys/types.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GET_TID                             \
    pthread_self() 

#define GC_CREATE                           \
    pthread_t __my_tid = pthread_self();    \
    gc_create(__my_tid);

#define GC_MALLOC(x)                        \
    gc_malloc(__my_tid, (x));

#define GC_RUN                              \
    gc_run(__my_tid);

#define GC_MARK_ROOT(var)                   \
    gc_mark_root(__my_tid, (void*)&(var));

#define GC_UNMARK_ROOT(var)                 \
    gc_unmark_root(__my_tid, (void*)&(var));


void gc_create(pthread_t tid);

void* gc_malloc(pthread_t tid, size_t size);

void gc_free(pthread_t tid, void* ptr);

void gc_run(pthread_t tid);

void gc_mark_root(pthread_t tid, void* addr);

void gc_unmark_root(pthread_t tid, void* addr);

#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H
