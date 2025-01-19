#include "gc/gc.h"
#include "gc/log.h"

#include <unordered_map>
#include <thread>
#include <csetjmp>

enum class ETAG {
    NONE,
    USED,
};

struct Allco_info {
    void* ptr;
    size_t size;
    ETAG tag;
};

struct Gc
{
    std::unordered_map<void*, Allco_info*> alloc_reg;
    void* bos = NULL;
    std::mutex gc_mtx;
};


std::unordered_map<pthread_t, Gc*> gc_reg;
std::mutex gc_reg_mtx;

void gc_create(pthread_t tid, void* bos) {
    std::lock_guard lock(gc_reg_mtx);
    gc_reg.insert({tid, new Gc});
    gc_reg[tid]->bos = bos;
}

void* gc_malloc(pthread_t tid, size_t size) {
    std::unique_lock lock(gc_reg_mtx);
    Gc* gc_ptr = gc_reg[tid];
    lock.unlock();

    std::lock_guard gc_lock(gc_ptr->gc_mtx);
    Allco_info* new_alloc = new Allco_info;
    new_alloc->size = size;
    new_alloc->tag = ETAG::NONE;
    new_alloc->ptr = malloc(size);

    gc_ptr->alloc_reg.insert({new_alloc->ptr, new_alloc});
    // LOG_PRINTF("MALLOC %p", new_alloc->ptr);
    return new_alloc->ptr;
}

void gc_free(pthread_t tid, void* ptr) {
    std::unique_lock lock(gc_reg_mtx);
    Gc* gc_ptr = gc_reg[tid];
    lock.unlock();

    std::lock_guard gc_lock(gc_ptr->gc_mtx);
    if (gc_ptr->alloc_reg.contains(ptr)) {
        Allco_info* alloc_info = gc_ptr->alloc_reg[ptr];
        gc_ptr->alloc_reg.erase(ptr);

        free(alloc_info->ptr);
        delete alloc_info;
    }
}

void gc_sweep(Gc* gc) {
    LOG_PRINTF("======== SWEEP PHASE ========");

    for (auto itr = gc->alloc_reg.begin(); itr != gc->alloc_reg.end();)
    {
        // Allco_info* info = itr->second;
        if (itr->second->tag == ETAG::NONE)
       {
            LOG_PRINTF("Sweep %p", itr->first);
            free(itr->first);
            delete itr->second;

            itr = gc->alloc_reg.erase(itr);
        } else {
            ++itr;
        }
    }
    
    for (const auto& alloc : gc->alloc_reg)
    {
        alloc.second->tag = ETAG::NONE;
    }
}

void gc_mark_alloc(Gc* gc, Allco_info* alloc) {
    if (alloc->tag == ETAG::USED)
    {
        return;
    }
    alloc->tag = ETAG::USED;
    
    char* alloc_end = (char*)alloc->ptr + alloc->size;
    for (char* ptr = (char*)alloc->ptr; ptr < alloc_end; ++ptr)
    {
        if (gc->alloc_reg.contains(*(void**)ptr))
        {
            LOG_PRINTF("Found %p on HEAP at %p", *(void**)ptr, ptr);
            gc_mark_alloc(gc, gc->alloc_reg[*(void**)ptr]);
        }
    }
}

void gc_mark_stack(Gc* gc, void* tos) {
    LOG_PRINTF("======== MARK PHASE ========");

    char* bos = (char*)gc->bos;
    for (char* ptr = (char*)tos; ptr < bos; ++ptr)
    {
        if (gc->alloc_reg.contains(*(void**)ptr))
        {
            LOG_PRINTF("Found %p on STACK at %p", *(void**)ptr, ptr);
            gc_mark_alloc(gc, gc->alloc_reg[*(void**)ptr]);
        }
    }
    
}

void gc_mark_registers(Gc* gc) {
    std::jmp_buf env;
    setjmp(env);

    for (char* ptr = (char*)&env; ptr < (char*)&env + sizeof(env); ++ptr) {
        if (gc->alloc_reg.contains(*(void**)ptr))
        {
            LOG_PRINTF("Found %p on REGISTERS at %p", *(void**)ptr, ptr);
            gc_mark_alloc(gc, gc->alloc_reg[*(void**)ptr]);
        }
    }
}

void gc_run(pthread_t tid) {
    std::unique_lock lock(gc_reg_mtx);
    if (gc_reg.contains(tid))
    {
        Gc* gc_ptr = gc_reg[tid];
        lock.unlock();

        size_t offset = (char*)pthread_get_stackaddr_np(tid) - (char*)gc_ptr->bos;
        void* tos = (char*)gc_ptr->bos - pthread_get_stacksize_np(tid) + offset + 1;

        gc_mark_registers(gc_ptr);
        // gc_mark_stack(gc_ptr, __builtin_frame_address(0));
        gc_mark_stack(gc_ptr, tos);
        gc_sweep(gc_ptr);
        return;
    }
    lock.unlock();
}


void* thread_gc_run(void* arg) {
    pthread_t tid = *(pthread_t*)arg;

    gc_run(tid);

    return NULL;
}