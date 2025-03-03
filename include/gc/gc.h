// #ifndef GC_PROJECT_GC_H
// #define GC_PROJECT_GC_H

// #ifdef __cplusplus
// extern "C" {
// #endif
// #include <sys/types.h>
// #include <pthread.h>

// #define GET_TID                             \
//     pthread_self() 

// #define GC_CREATE                           \
//     pthread_t __my_tid = pthread_self();    \
//     gc_create(__my_tid);

// #define GC_MALLOC(x)                        \
//     gc_malloc(__my_tid, (x));

// #define GC_RUN                              \
//     gc_run(__my_tid);

// #define GC_MARK_ROOT(var)                   \
//     gc_mark_root(__my_tid, (void*)&(var));

// #define GC_UNMARK_ROOT(var)                 \
//     gc_unmark_root(__my_tid, (void*)&(var));

// #define GC_STOP                             \
//     gc_stop(__my_tid);

// void gc_create(pthread_t tid);

// void* gc_malloc(pthread_t tid, size_t size);

// void gc_free(pthread_t tid, void* ptr);

// void gc_run(pthread_t tid);

// void gc_mark_root(pthread_t tid, void* addr);

// void gc_unmark_root(pthread_t tid, void* addr);

// void gc_stop(pthread_t tid);

// #ifdef __cplusplus
// }
// #endif

// #endif //GC_PROJECT_GC_H



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

gc_handler* gc_create(pthread_t tid);
void gc_stop(pthread_t tid);

void handle_sigusr1(int sig);


#define GC_CREATE()                                                 \
    gc_handler* __handler = gc_create(pthread_self());

#define GC_MALLOC(val, size)                                        \
    __handler->gc_malloc(pthread_self(), (void**)(val), (size));

#define GC_MARK_ROOT(val)                                           \
    __handler->mark_root(pthread_self(), (void*)(&(val)));

#define GC_COLLECT(flag)                                            \
    __handler->collect(pthread_self(), (flag));

#define GC_STOP()                                                   \
    gc_stop(pthread_self());

#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H



