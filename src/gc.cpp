#include <iostream>
#include <unordered_map>
#include <thread>
#include <pthread.h>
#include "gc/gc.h"

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


    ~Allocation() {
        free(ptr);
    }
};

class Garbage_collector{
private:
    pthread_t tid_;
    std::unordered_map<void*, Allocation*> mem_reg_;
    bool stop_;
    bool quit_;


    void run_mark_phase() {

    }

    void run_sweep_phase() {

    }
public:
    Garbage_collector(pthread_t tid) : tid_(tid) {
        stop_ = quit_ = true;
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
        
        delete mem_reg_[ptr];
    }

    void run() {
        while (!quit_)
        {
            
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
    holder->context = gc_ptr;

    
    std::thread gc_thread([](Garbage_collector* gc_ptr){
        gc_ptr->run();
    }, gc_ptr);
    gc_thread.detach();
    
    return holder;
}

