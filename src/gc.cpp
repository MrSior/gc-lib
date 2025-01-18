// #include <iostream>
// #include <unordered_map>
// #include <thread>
// #include <chrono>
// #include <pthread.h>
// #include "gc/gc.h"
// #include "gc/log.h"

// constexpr int PTRSIZE = sizeof(char*); 

// enum class ETAG : uint8_t {
//     USED,
//     NONE,
// };

// /**
//  * @struct Allocation
//  * @brief Represents a memory allocation with additional metadata for management.
//  */
// struct Allocation
// {
//     /**
//      * @brief Pointer to the allocated memory block.
//      */
//     void* ptr;

//     /**
//      * @brief Tag used during the mark phase of garbage collection or memory management.
//      * 
//      * This tag helps to identify the state of the memory block.
//      */
//     ETAG tag;

//     /**
//      * @brief Size of the allocated memory block, in bytes.
//      */
//     size_t size;

//     char* get_mem_block_end() {
//         return (char*)ptr + size;
//     }


//     ~Allocation() {
//         free(ptr);
//     }
// };

// class Garbage_collector{
// private:
//     pthread_t tid_;                                     // tid of thread which gc track 
//     std::unordered_map<void*, Allocation*> mem_reg_;    
//     bool stop_;
//     bool quit_;

//     void mark_alloc(Allocation* alloc_ptr) {
//         if (alloc_ptr->tag == ETAG::USED)
//         {
//             return;
//         }
        
//         LOG_PRINTF("Scaning block at heap [%p; %p] size: %lu ...", alloc_ptr->ptr, alloc_ptr->get_mem_block_end(), alloc_ptr->size);

//         alloc_ptr->tag = ETAG::USED;
//         for (char* ptr = (char*)alloc_ptr->ptr; ptr < alloc_ptr->get_mem_block_end(); ptr += PTRSIZE)
//         {
//             if (mem_reg_.contains(*(void**)ptr))
//             {
//                 LOG_PRINTF("Found address %p located on heap at %p", *(void**)ptr, ptr);

//                 mark_alloc(mem_reg_[*(void**)ptr]);
//             }
//         }
//     }

//     void mark_stack() {
//         void *bos, *tos;
//         if (get_stack_bounds(&bos, &tos, tid_) == 0)
//         {
//             // TODO: throw some exception or do nothing
//         }
//         LOG_PRINTF("Scaning stack in [%p ; %p] ...", bos, tos);
//         for(char* ptr = (char*)bos; ptr < tos; ptr += PTRSIZE) {
//             if (mem_reg_.contains(*(void**)ptr))
//             {
//                 LOG_PRINTF("Found address %p located on stack at %p", *(void**)ptr, ptr);
//                 // mark_alloc(mem_reg_[*(void**)ptr]);
//             }
//         }
//     }

//     void run_mark_phase() {
//         LOG_PRINTF("========= MARK PHASE =========");
//         mark_stack();
//     }

//     void sweep(Allocation* alloc_ptr) {
//         if (alloc_ptr->tag == ETAG::USED)
//         {
//             return;
//         }
        
//         LOG_PRINTF("Found LEAKED ADDRESS %p", alloc_ptr->ptr);
//         LOG_PRINTF("Scannin block at heap [%p; %p] size: %lu for LEAKS ...", alloc_ptr->ptr, alloc_ptr->get_mem_block_end(), alloc_ptr->size);

//         for (char* ptr = (char*)alloc_ptr->ptr; ptr < alloc_ptr->get_mem_block_end(); ptr += PTRSIZE) {
//             if (mem_reg_.contains(*(void**)ptr))
//             {
//                 sweep(mem_reg_[*(void**)ptr]);
//             }
//         }

//         // Garbage_collector::free(alloc_ptr->ptr);
//     }

//     void run_sweep_phase() {
//         LOG_PRINTF("========= SWEEP PHASE =========");
//         for (const auto& elem : mem_reg_)
//         {
//             sweep(elem.second);
//         }

//         for (auto &&elem : mem_reg_)
//         {
//             elem.second->tag = ETAG::NONE;
//         }
//     }
// public:
//     Garbage_collector(pthread_t tid) : tid_(tid) {
//         stop_ = quit_ = false;
//     };

//     void* malloc(size_t size) {
//         void* ptr = ::malloc(size);
//         Allocation* new_alloc = new Allocation{ptr, ETAG::NONE, size};
//         mem_reg_.insert({ptr, new_alloc});
//         ptr = NULL;
//         return new_alloc->ptr;
//     }

//     void free(void* ptr) {
//         if (!mem_reg_.contains(ptr))
//         {
//             // TODO: throw some exception or do nothing
//             return;
//         }
        
//         delete mem_reg_[ptr];
//         mem_reg_.erase(ptr);
//     }

//     void run() {
//         std::this_thread::sleep_for(std::chrono::seconds(5));
//         LOG_PRINTF("Start gc");
//         while (!quit_)
//         {
//             run_mark_phase();
//             // run_sweep_phase();
//             quit_ = true;
//         }
//     }

//     ~Garbage_collector() {
//         // for (const auto& elem : mem_reg_)
//         // {
//         //     delete elem.second;
//         // }
//         LOG_PRINTF("GC DIED");
//     }
// };

// void* gc_malloc_wrapper(void* obj, size_t size) {
//     return static_cast<Garbage_collector*>(obj)->malloc(size);
// }

// void gc_free_wrapper(void* obj, void* ptr) {
//     static_cast<Garbage_collector*>(obj)->free(ptr);
// }

// static std::unordered_map<pthread_t, Garbage_collector*> gc_reg;
// static std::mutex gc_reg_mtx;
// static std::atomic<bool> need_clean;

// gc_holder* create_gc(pthread_t tid) {
//     if (gc_reg.contains(tid))
//     {
//         // TODO: logic for this case
//     }
    
//     Garbage_collector* gc_ptr = new Garbage_collector(tid);

//     gc_reg[tid] = gc_ptr;
//     gc_holder* holder =  new gc_holder();
    
//     holder->malloc = &gc_malloc_wrapper;
//     holder->free = &gc_free_wrapper;
//     holder->ctx = gc_ptr;

    
//     std::thread gc_thread([](Garbage_collector* gc_ptr){
//         gc_ptr->run();
//     }, gc_ptr);
//     gc_thread.detach();
    
//     return holder;
// }


#include "gc/gc.h"
#include "gc/log.h"

#include <unordered_map>
#include <thread>

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
};


static std::unordered_map<pthread_t, Gc*> gc_reg_;
static std::mutex gc_reg_mtx;

void gc_create(pthread_t tid, void* bos) {
    std::lock_guard lock(gc_reg_mtx);
    gc_reg_.insert({tid, new Gc});
    gc_reg_[tid]->bos = bos;
}

void* gc_malloc(pthread_t tid, size_t size) {
    std::lock_guard lock(gc_reg_mtx);
    Gc* gc_ptr = gc_reg_[tid];

    Allco_info* new_alloc = new Allco_info;
    new_alloc->size = size;
    new_alloc->tag = ETAG::NONE;
    new_alloc->ptr = malloc(size);

    gc_ptr->alloc_reg.insert({new_alloc->ptr, new_alloc});
    return new_alloc->ptr;
}

void gc_free(pthread_t tid, void* ptr) {
    std::lock_guard lock(gc_reg_mtx);
    Gc* gc_ptr = gc_reg_[tid];

    if (gc_ptr->alloc_reg.contains(ptr)) {
        Allco_info* alloc_info = gc_ptr->alloc_reg[ptr];
        gc_ptr->alloc_reg.erase(ptr);

        free(alloc_info->ptr);
        delete alloc_info;
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

void gc_run(pthread_t tid) {
    if (gc_reg_.contains(tid))
    {
        gc_mark_stack(gc_reg_[tid], __builtin_frame_address(0));
    }
}