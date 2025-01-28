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
    void*(*gc_malloc)(pthread_t, size_t);
    void(*gc_free)(pthread_t, void*);
    void(*mark_root)(pthread_t, void*);
    void(*unmark_root)(pthread_t, void*);
    void(*collect)(pthread_t, int);
} gc_handler;

gc_handler* gc_create(pthread_t tid);


pthread_mutex_t global_run_threads_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t global_run_threads_cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t global_run_manager_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t global_run_manager_cv = PTHREAD_COND_INITIALIZER;
int global_run_finished_flag = 0;
size_t handle_sig_cnt = 0;
void handle_sigusr1(int sig) {
    if (sig == SIGUSR1)
    {   
        printf("I am stopped\n");
    
        pthread_mutex_lock(&global_run_manager_mtx);
        ++handle_sig_cnt;
        pthread_cond_signal(&global_run_manager_cv);
        pthread_mutex_unlock(&global_run_manager_mtx);

        pthread_mutex_lock(&global_run_threads_mtx);
        global_run_finished_flag = 0;

        printf("waiting to be woken up\n");
        while (global_run_finished_flag == 0)
        {
            pthread_cond_wait(&global_run_threads_cv, &global_run_threads_mtx);
        }
        pthread_mutex_unlock(&global_run_threads_mtx);

        printf("I am woken up after collection\n");
    }
}

void stop_world_sig_init() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigusr1;
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H



