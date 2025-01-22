#include "gc/gc.h"
#include "gc/log.h"

#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <csetjmp>
#include <unistd.h>

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
    std::unordered_set<void*> roots;
    std::mutex mtx;
    std::atomic<bool> is_terminate = false;
};

void* gc_schedual_run(void* arg);

class Gc_registry {
    std::unordered_map<pthread_t, Gc*> reg_;
    std::mutex reg_mtx_;
public:
    void add(pthread_t tid) {
        std::lock_guard lock(reg_mtx_);
        reg_.insert({tid, new Gc});

        pthread_t gc_thread;
        pthread_t* arg = new pthread_t(tid);
        if (pthread_create(&gc_thread, NULL, gc_schedual_run, arg))
        {
            perror("pthread_create failed");
            return;
        }
        pthread_detach(gc_thread);
    }

    Gc* get(pthread_t tid) {
        std::lock_guard lock(reg_mtx_);
        if (reg_.contains(tid))
        {
            return reg_[tid];
        }
        throw std::invalid_argument("Try to get gc that doesn't exist");
    }

    void erase(pthread_t tid) {
        std::lock_guard lock(reg_mtx_);
        if (reg_.contains(tid))
        {
            reg_.erase(tid);
        }
        throw std::invalid_argument("Try to erase gc that doesn't exist");
    }
};

static Gc_registry gc_reg;

void* gc_schedual_run(void* arg) {
    if (arg == NULL)
    {
        throw std::invalid_argument("Tid_ptr is NULL");
    }
    
    pthread_t observed_tid = *(pthread_t*)(arg);
    delete (pthread_t*)arg;
    Gc* gc_ptr = gc_reg.get(observed_tid);
    
    while (!gc_ptr->is_terminate.load())
    {
        sleep(2);
        gc_run(observed_tid);
    }
    LOG_PRINTF("GC KILLED");
    return NULL;
}

void gc_create(pthread_t tid) {
    gc_reg.add(tid);
}

void* gc_malloc(pthread_t tid, size_t size) {
    Gc* gc_ptr = gc_reg.get(tid);

    std::lock_guard gc_lock(gc_ptr->mtx);
    Allco_info* new_alloc = new Allco_info;
    new_alloc->size = size;
    new_alloc->tag = ETAG::NONE;
    new_alloc->ptr = malloc(size);

    gc_ptr->alloc_reg.insert({new_alloc->ptr, new_alloc});
    // LOG_PRINTF("MALLOC %p", new_alloc->ptr);
    return new_alloc->ptr;
}

void gc_free(pthread_t tid, void* ptr) {
    Gc* gc_ptr = gc_reg.get(tid);

    std::lock_guard gc_lock(gc_ptr->mtx);
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
    LOG_PRINTF("=============================");
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

void gc_mark_root(pthread_t tid, void* addr) {
    Gc* gc_ptr = gc_reg.get(tid);
    std::lock_guard gc_lock(gc_ptr->mtx);
    gc_ptr->roots.insert(addr);
}

void gc_unmark_root(pthread_t tid, void* addr) {
    Gc* gc_ptr = gc_reg.get(tid);
    std::lock_guard gc_lock(gc_ptr->mtx);
    gc_ptr->roots.erase(addr);
}



void gc_run(pthread_t tid) {
    Gc* gc_ptr = gc_reg.get(tid);
    std::lock_guard gc_lock(gc_ptr->mtx);
    for (const auto& ptr: gc_ptr->roots)
    {
        if (gc_ptr->alloc_reg.contains(*(void**)ptr))
        {
            LOG_PRINTF("Found %p on STACK at %p", *(void**)ptr, ptr);
            gc_mark_alloc(gc_ptr, gc_ptr->alloc_reg[*(void**)ptr]);   
        } else {
            LOG_PRINTF("ROOT %p lost address", ptr);
        }
    }
    
    gc_sweep(gc_ptr);
}

void gc_stop(pthread_t tid) {
    Gc* gc_ptr = gc_reg.get(tid);
    gc_ptr->is_terminate.store(true);
    gc_sweep(gc_ptr);
}