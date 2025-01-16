#ifndef GC_PROJECT_GC_H
#define GC_PROJECT_GC_H
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef struct gc_holder {
    void*(*malloc)(void* obj, size_t);
    void(*free)(void* obj, void* ptr);
    void* ctx;
} gc_holder; 


gc_holder* create_gc(pthread_t tid);


#if defined(__linux__)
    #define _GNU_SOURCE
    #include <sys/sysinfo.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
#endif

int get_stack_bounds(void **stack_bottom, void **stack_top, pthread_t tid) {
    pthread_t thread = tid;

#if defined(__linux__)
    // Linux: используем pthread_getattr_np
    pthread_attr_t attr;
    if (pthread_getattr_np(thread, &attr) != 0) {
        perror("pthread_getattr_np");
        return -1;
    }

    void *stack_addr;
    size_t stack_size;
    if (pthread_attr_getstack(&attr, &stack_addr, &stack_size) != 0) {
        perror("pthread_attr_getstack");
        pthread_attr_destroy(&attr);
        return -1;
    }

    *stack_bottom = stack_addr;
    *stack_top = (void *)((char *)stack_addr + stack_size);

    pthread_attr_destroy(&attr);
    return 0;

#elif defined(__APPLE__)
    // macOS: используем pthread_get_stackaddr_np и pthread_get_stacksize_np
    void *stack_addr = pthread_get_stackaddr_np(thread);
    size_t stack_size = pthread_get_stacksize_np(thread);

    if (stack_addr == NULL || stack_size == 0) {
        fprintf(stderr, "Failed to retrieve stack information\n");
        return -1;
    }

    // На macOS стек растёт вниз
    *stack_top = stack_addr;
    *stack_bottom = (void *)((char *)stack_addr - stack_size);

    return 0;

#else
    // Для других ОС выдаём ошибку
    fprintf(stderr, "Unsupported platform\n");
    return -1;
#endif
}


#ifdef __cplusplus
}
#endif

#endif //GC_PROJECT_GC_H
