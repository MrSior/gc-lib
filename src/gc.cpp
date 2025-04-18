#include "gc/gc.h"
#include "gc/log.h"
#include "gc/thread-pool.h"

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <csetjmp>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#undef LOG_LVL
#define LOG_LVL LOG_LEVEL::INFO

#define MAX_MEM_CAPACITY UINT64_MAX
#define INITIAL_SWEEP_FACTOR 1024

enum class ETAG {
    NONE,
    USED,
};

enum class EERROR {
    NONE,
    NOMEM,
};

struct alloc_info
{
    void* addr;
    size_t size;
    ETAG tag;
};

std::atomic<bool> is_global_collecting = false;
std::atomic<bool>            is_stoped = false;

std::condition_variable gr_manager_cv; // global run manager cv
std::mutex              gr_manager_mtx;

std::condition_variable handle_cv;
std::mutex              handle_mtx;

void handle_sigusr1(int sig) {
    if (sig == SIGUSR1)
    {   
        LOG_DEBUG("%s", "I am stopped");
        is_stoped.store(true);
        gr_manager_cv.notify_one();

        std::unique_lock handle_lock(handle_mtx);
        LOG_DEBUG("%s", "I am fall a sleep");
        handle_cv.wait(handle_lock, []() -> bool {
            return !is_global_collecting.load();
        });
        LOG_DEBUG("%s", "I am woken up");
    }
}

class gc {
private:
    uint64_t sweep_factor;
    uint64_t cur_mem_capacity;

    std::unordered_set<void*> roots_;
    std::unordered_map<void*, alloc_info*> allocs_reg_;

    void scan_allocation(alloc_info* alloc) {
        if (alloc->tag == ETAG::USED) { return; }
        
        alloc->tag = ETAG::USED;
        LOG_DEBUG("Mark %p", alloc->addr);

        for (char* mem_block = reinterpret_cast<char*>(alloc->addr);
             mem_block < reinterpret_cast<char*>(alloc->addr) + alloc->size;
             ++mem_block)
        {
            auto itr = allocs_reg_.find(*reinterpret_cast<void**>(mem_block));
            if (itr == allocs_reg_.end()) { continue; }
            scan_allocation(itr->second);
        }   
    }

    void mark() {
        for (const auto &root : roots_)
        {
            auto itr = allocs_reg_.find(*static_cast<void**>(root));
            if (itr != allocs_reg_.end())
            {
                scan_allocation(itr->second);
            }
        }
    }

    void sweep() {
        std::erase_if(allocs_reg_, [](const auto& item) -> bool {
            auto const& [key, value] = item;
            if (value->tag == ETAG::USED) { return false; }
            LOG_INFO("Sweep %p", key)
            free(key);
            delete value;
            return true;
        });

        std::for_each(allocs_reg_.begin(), allocs_reg_.end(), [](auto& item){
            auto& [key, value] = item;
            value->tag = ETAG::NONE;
        });
    }
public:
    unsigned long long int get_allocs_cnt() {
        return allocs_reg_.size();
    }

    unsigned long long int get_roots_cnt() {
        return roots_.size();
    }

    void gc_malloc(size_t size, void*& res, EERROR& error) {
        if (cur_mem_capacity >= sweep_factor)
        {
            LOG_INFO("%s", "GC backgroung collection");
            collect();
            if (MAX_MEM_CAPACITY / 2 > sweep_factor)
            {
                sweep_factor *= 2;
            }
        }
        
        res = malloc(size);

        if (errno == ENOMEM || errno == EAGAIN)
        {
            error = EERROR::NOMEM;
            return;
        }

        error = EERROR::NONE;
        alloc_info* allocation = new alloc_info;
        allocation->addr = res;
        allocation->size = size;
        allocation->tag = ETAG::NONE;

        allocs_reg_.insert({res, allocation});
        cur_mem_capacity += size;
        LOG_DEBUG("Malloc at %p size of %lu", res, size);
    }

    void gc_free(void* addr) {
        auto itr = allocs_reg_.find(addr);
        if (itr == allocs_reg_.end())
        {
            return;
        }
        
        LOG_DEBUG("Free %p", addr);
        cur_mem_capacity -= itr->second->size;

        free(addr);
        delete itr->second;
        allocs_reg_.erase(itr);
    }

    void mark_root(void* addr) {
        roots_.insert(addr);
    }

    void unmark_root(void* addr) {
        if (!roots_.contains(addr)) { return; }
        
        roots_.erase(addr);
    }

    void collect() {
        mark();
        sweep();
    }

    gc() {
        cur_mem_capacity = 0;
        sweep_factor = INITIAL_SWEEP_FACTOR;
    }

    ~gc() {
        for (const auto& alloc : allocs_reg_) {
            free(alloc.first);
            delete alloc.second;
        }
    }
};

class gc_manager
{
private:
    std::mutex global_run_mtx;
    std::mutex reg_mtx_;
    std::unordered_map<pthread_t, gc*> reg_;
    thread_pool tpool_;
    size_t gc_cnt;

    gc* get_gc(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        if (!reg_.contains(tid))
        {
            LOG_CRITICAL("Thread with id: %lld does not have GC", (long long int)tid);
            // std::exit(EXIT_FAILURE);
            errno = EINVAL;
            return NULL;
        }
        
        return reg_[tid];
    }

    void global_run(pthread_t origin_tid) {
        if (is_global_collecting.load()) { return; }
        is_global_collecting.store(true);
        std::lock_guard run_lock(global_run_mtx);
        LOG_INFO("%s", "Start global gc");
        tpool_.block();
        tpool_.wait_all();
        

        for (const auto&[key, val] : reg_) {
            if (key == origin_tid) { continue; }
            is_stoped.store(false);
            pthread_kill(key, SIGUSR1);

            std::unique_lock collection_lock(gr_manager_mtx);
            gr_manager_cv.wait(collection_lock, [](){
                return is_stoped.load();
            });
        }

        LOG_DEBUG("%s", "All threads sleep");

        LOG_DEBUG("Tpool q_mutex = %d", (int)tpool_.check_q_mutex());
        
        for (auto[key, val] : reg_) {
            LOG_DEBUG("%s", "Done 1 collect");
            // do_collect(key);
            tpool_.add_priority_task([val]() { val->collect(); });
        }
        tpool_.wait_all();

        LOG_DEBUG("%s", "Done cleaning")

        is_global_collecting.store(false);
        handle_cv.notify_all();
        tpool_.unblock();
        LOG_DEBUG("%s", "All threads are waking up")
    }

    void nomem_handler(pthread_t origin_tid, gc* thread_gc, void*& dest, size_t size) {
        if (is_global_collecting.load())
        {
            std::unique_lock handle_lock(handle_mtx);
            handle_cv.wait(handle_lock, []() -> bool {
                return !is_global_collecting.load();
            });       
        } else
        {
            global_run(origin_tid);
        }
        
        EERROR error;
        auto task_id = tpool_.add_task([thread_gc](size_t size, void*& res, EERROR& err) -> void {
                                            thread_gc->gc_malloc(size, res, err); 
                                        },
                                        size,
                                        std::ref(dest),
                                        std::ref(error));
        tpool_.wait(task_id);

        if (error == EERROR::NOMEM)
        {
            LOG_CRITICAL("%s", "Heap overflow");
            // std::exit(EXIT_FAILURE);
            errno = ENOMEM;
        }
    }
public:
    gc_manager() {
        gc_cnt = 0;
    }

    void add_to_reg(pthread_t tid, gc* new_gc) {
        std::lock_guard reg_lock(reg_mtx_);
        reg_.insert({tid, new_gc});
        ++gc_cnt;
        
        if (tpool_.get_threads_n() < gc_cnt)
        {
            tpool_.add_thread();
            LOG_DEBUG("%s", "Added 1 thread to thread pool");
        }
    }

    bool contains(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        return reg_.contains(tid);
    }

    void erase_from_reg(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        auto itr = reg_.find(tid);
        if (itr == reg_.end()) return;
        delete itr->second;
        reg_.erase(itr);
    }

    unsigned long long int get_gc_allocs_cnt(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        auto itr = reg_.find(tid);
        return itr->second->get_allocs_cnt();
    }

    unsigned long long int gel_all_threads_allocs_cnt() {
        std::lock_guard reg_lock(reg_mtx_);
        unsigned long long int sum = 0;
        for (const auto& [key, gc] : reg_)
        {
            sum += gc->get_allocs_cnt();
        }
        return sum;
    }

    unsigned long long int get_gc_roots_cnt(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        auto itr = reg_.find(tid);
        return itr->second->get_roots_cnt();
    }

    void do_malloc(pthread_t tid, void*& dest, size_t size) {
        gc* thread_gc = get_gc(tid);
        if (thread_gc == NULL) { return; }
        
        
        EERROR error;
        auto task_id = tpool_.add_task([thread_gc](size_t size, void*& res, EERROR& err) -> void {
                                            thread_gc->gc_malloc(size, res, err); 
                                        },
                                        size,
                                        std::ref(dest),
                                        std::ref(error));
        tpool_.wait(task_id);
        
        if (error == EERROR::NOMEM)
        {
            nomem_handler(tid, thread_gc, dest, size);
        }
        
    }

    void do_free(pthread_t tid, void* addr) {
        gc* thread_gc = get_gc(tid);
        if (thread_gc == NULL) { return; }

        auto task_id = tpool_.add_task([thread_gc](void* addr) { thread_gc->gc_free(addr); }, addr);
        tpool_.wait(task_id);
    }

    void do_root_marking(pthread_t tid, void* addr) {
        gc* thread_gc = get_gc(tid);
        if (thread_gc == NULL) { return; }

        auto task_id = tpool_.add_task([thread_gc](void* addr) { thread_gc->mark_root(addr); }, addr);
        tpool_.wait(task_id);
    }

    void do_root_unmarking(pthread_t tid, void* addr) {
        gc* thread_gc = get_gc(tid);
        if (thread_gc == NULL) { return; }

        auto task_id = tpool_.add_task([thread_gc](void* addr) { thread_gc->unmark_root(addr); }, addr);
        tpool_.wait(task_id);
    }

    void do_collect(pthread_t tid, int flag = THREAD_LOCAL) {
        if (flag == GLOBAL)
        {   
            global_run(tid);
            return;
        } else if (flag == THREAD_LOCAL)
        {
            gc* thread_gc = get_gc(tid);
            if (thread_gc == NULL) { return; }

            auto task_id = tpool_.add_task([thread_gc]() { thread_gc->collect(); });
            tpool_.wait(task_id);
        } else {
            errno = EINVAL;
        }
    }
};

static gc_manager manager;


void manager_malloc_wrapper(pthread_t tid, void** dest, size_t size) {
    LOG_DEBUG("Malloc destination: %p", dest);
    manager.do_malloc(tid, *dest, size);
}

void manager_free_wrapper(pthread_t tid, void* addr) {
    manager.do_free(tid, addr);
}

void manager_mark_root__wrapper(pthread_t tid, void* addr) {
    manager.do_root_marking(tid, addr);
}

void manager_unmark_root_wrapper(pthread_t tid, void* addr) {
    manager.do_root_unmarking(tid, addr);
}

void manager_collect_wrapper(pthread_t tid, int flag) {
    manager.do_collect(tid, flag);
}

void stop_world_sig_init() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigusr1;
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        char buf[256];
        if (strerror_r(errno, buf, sizeof(buf)) != 0) {
            LOG_CRITICAL("%s", "Sigaction: Failed to bind SIGUSR1");
            return;
        }
        LOG_CRITICAL("Sigaction: Failed to bind SIGUSR1: %s", buf);
        // exit(EXIT_FAILURE);
    }
}

gc_handler gc_get_handler() {
    gc_handler handler;

    handler.gc_malloc = &manager_malloc_wrapper;
    handler.gc_free = &manager_free_wrapper;
    handler.mark_root = &manager_mark_root__wrapper;
    handler.unmark_root = &manager_unmark_root_wrapper;
    handler.collect = &manager_collect_wrapper;

    return handler;
}

gc_handler gc_create(pthread_t tid) {
    if (manager.contains(tid))
    {
        LOG_WARNING("Thread with id: %lld already has GC", (long long int)tid);
        gc_handler handler;
        return handler;
    }

    stop_world_sig_init();
    if (errno == EFAULT || errno == EINVAL)
    {
        LOG_CRITICAL("%s", "Failed to create GC due to error in sigaction");
        return gc_get_handler();
    }

    manager.add_to_reg(tid, new gc);

    return gc_get_handler();
}

void gc_stop(pthread_t tid) {
    manager.erase_from_reg(tid);
}

unsigned long long int gc_get_allocs_cnt(pthread_t tid) {
    if (!manager.contains(tid))
    {
        LOG_CRITICAL("Thread with id: %lld does not have GC", (long long int)tid);
        // exit(EXIT_FAILURE);
        errno = EINVAL;
        return 0;
    }
    return manager.get_gc_allocs_cnt(tid);
}

unsigned long long int gc_gel_all_threads_allocs_cnt() {
    return manager.gel_all_threads_allocs_cnt();
}

unsigned long long int gc_get_roots_cnt(pthread_t tid) {
    if (!manager.contains(tid))
    {
        LOG_CRITICAL("Thread with id: %lld does not have GC", (long long int)tid);
        // exit(EXIT_FAILURE);
        errno = EINVAL;
        return 0;
    }
    return manager.get_gc_roots_cnt(tid);
}