#ifndef GC_PROJECT_GC_H
#define GC_PROJECT_GC_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define GLOBAL 1
#define THREAD_LOCAL 0

typedef struct gc_handler
{
    void(*gc_malloc)(pthread_t, void**, size_t);
    void(*gc_free)(pthread_t, void*);
    void(*mark_root)(pthread_t, void*);
    void(*unmark_root)(pthread_t, void*);
    void(*collect)(pthread_t, int);
} gc_handler;

gc_handler gc_create(pthread_t tid);
gc_handler gc_get_handler();
void gc_stop(pthread_t tid);
unsigned long long int gc_get_allocs_cnt(pthread_t tid);
unsigned long long int gc_get_roots_cnt(pthread_t tid);
unsigned long long int gc_gel_all_threads_allocs_cnt();

void handle_sigusr1(int sig);


#define GC_CREATE()                                                         \
    gc_create(pthread_self());

#define GC_MALLOC(val, size)                                                \
    gc_get_handler().gc_malloc(pthread_self(), (void**)(&(val)), (size));

#define GC_MARK_ROOT(val)                                                   \
    gc_get_handler().mark_root(pthread_self(), (void*)(&(val)));

#define GC_UNMARK_ROOT(val)                                                 \
    gc_get_handler().unmark_root(pthread_self(), (void*)(&(val)));    

#define GC_COLLECT(flag)                                                    \
    gc_get_handler().collect(pthread_self(), (flag));

#define GC_STOP()                                                           \
    gc_stop(pthread_self());

#define GC_FREE(ptr)                                                        \
    gc_get_handler().gc_free(pthread_self(), (void*)(ptr));

#define GC_GET_ALLOCS_CNT()                                                 \
    gc_get_allocs_cnt(pthread_self());

#define GC_GET_ROOTS_CNT()                                                  \
    gc_get_roots_cnt(pthread_self());

#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H



