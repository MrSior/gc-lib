#include <iostream>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <pthread.h>
#include "gc/gc.h"
#include "gc/log.h"

constexpr int PTRSIZE = sizeof(char*); 

enum class ETAG : uint8_t {
    USED,
    NONE,
};

/**
 * @struct Allocation
 * @brief Represents a memory allocation with additional metadata for management.
 */
struct Allocation
{
    /**
     * @brief Pointer to the allocated memory block.
     */
    void* ptr;

    /**
     * @brief Tag used during the mark phase of garbage collection or memory management.
     * 
     * This tag helps to identify the state of the memory block.
     */
    ETAG tag;

    /**
     * @brief Size of the allocated memory block, in bytes.
     */
    size_t size;

    char* get_mem_block_end() {
        return (char*)ptr + size;
    }


    ~Allocation() {
        free(ptr);
    }
};

class Garbage_collector{
private:
    pthread_t tid_;                                     // tid of thread which gc track 
    std::unordered_map<void*, Allocation*> mem_reg_;    
    bool stop_;
    bool quit_;

    void mark_alloc(Allocation* alloc_ptr) {
        if (alloc_ptr->tag == ETAG::USED)
        {
            return;
        }
        
        LOG_PRINTF("Scaning block at heap [%p; %p] size: %lu ...", alloc_ptr->ptr, alloc_ptr->get_mem_block_end(), alloc_ptr->size);

        alloc_ptr->tag = ETAG::USED;
        for (char* ptr = (char*)alloc_ptr->ptr; ptr < alloc_ptr->get_mem_block_end(); ptr += PTRSIZE)
        {
            if (mem_reg_.contains(*(void**)ptr))
            {
                LOG_PRINTF("Found address %p located on heap at %p", *(void**)ptr, ptr);

                mark_alloc(mem_reg_[*(void**)ptr]);
            }
        }
    }

    void mark_stack() {
        void *bos, *tos;
        if (get_stack_bounds(&bos, &tos, tid_) == 0)
        {
            // TODO: throw some exception or do nothing
        }
        LOG_PRINTF("Scaning stack in [%p ; %p] ...", bos, tos);
        for(char* ptr = (char*)bos; ptr < tos; ptr += PTRSIZE) {
            if (mem_reg_.contains(*(void**)ptr))
            {
                LOG_PRINTF("Found address %p located on stack at %p", *(void**)ptr, ptr);
                mark_alloc(mem_reg_[*(void**)ptr]);
            }
        }
    }

    void run_mark_phase() {
        mark_stack();
    }

    void sweep(Allocation* alloc_ptr) {

    }

    void run_sweep_phase() {
        for (const auto& elem : mem_reg_)
        {
            if (elem.second->tag == ETAG::NONE)
            {
                delete elem.second;
                mem_reg_.erase(elem.first);
            }
        }

        for (auto &&elem : mem_reg_)
        {
            elem.second->tag = ETAG::NONE;
        }
    }
public:
    Garbage_collector(pthread_t tid) : tid_(tid) {
        stop_ = quit_ = false;
    };

    void* malloc(size_t size) {
        void* ptr = ::malloc(size);
        Allocation* new_alloc = new Allocation{ptr, ETAG::NONE, size};
        mem_reg_.insert({ptr, new_alloc});
        return new_alloc->ptr;
    }

    void free(void* ptr) {
        if (!mem_reg_.contains(ptr))
        {
            // TODO: throw some exception or do nothing
        }
        
        // delete mem_reg_[ptr];
    }

    void run() {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        LOG_PRINTF("Start gc");
        while (!quit_)
        {
            run_mark_phase();
            run_sweep_phase();
            quit_ = true;
        }
    }

    ~Garbage_collector() {
        for (const auto& elem : mem_reg_)
        {
            delete elem.second;
        }
        
    }
};

void* gc_malloc_wrapper(void* obj, size_t size) {
    return static_cast<Garbage_collector*>(obj)->malloc(size);
}

void gc_free_wrapper(void* obj, void* ptr) {
    static_cast<Garbage_collector*>(obj)->free(ptr);
}

static std::unordered_map<pthread_t, Garbage_collector*> gc_reg;
static std::mutex gc_reg_mtx;
static std::atomic<bool> need_clean;

gc_holder* create_gc(pthread_t tid) {
    if (gc_reg.contains(tid))
    {
        // TODO: logic for this case
    }
    
    Garbage_collector* gc_ptr = new Garbage_collector(tid);

    gc_reg[tid] = gc_ptr;
    gc_holder* holder =  new gc_holder();
    
    holder->malloc = &gc_malloc_wrapper;
    holder->free = &gc_free_wrapper;
    holder->ctx = gc_ptr;

    
    std::thread gc_thread([](Garbage_collector* gc_ptr){
        gc_ptr->run();
    }, gc_ptr);
    gc_thread.detach();
    
    return holder;
}



// ??    -< 0x16fd0ed30


// A_ptr -< 0x16fd0efa8