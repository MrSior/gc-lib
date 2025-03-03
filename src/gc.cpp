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

#undef LOG_LVL
#define LOG_LVL LOG_LEVEL::DEBUG

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
            if (itr == allocs_reg_.end()) { return; }
            scan_allocation(itr->second);
        }   
    }

    void mark() {
        for (auto &&root : roots_)
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
    void gc_malloc(size_t size, void*& res, EERROR& error) {
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
    }

    void gc_free(void* addr) {
        auto itr = allocs_reg_.find(addr);
        if (itr == allocs_reg_.end())
        {
            return;
        }
        
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

    gc* get_gc(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        if (!reg_.contains(tid))
        {
            LOG_CRITICAL("%s", "Try to get gc by wrong tid");
            std::exit(EXIT_FAILURE);
        }
        
        return reg_[tid];
    }

    void global_run(pthread_t origin_tid) {
        if (is_global_collecting.load()) { return; }
        is_global_collecting.store(true);
        std::lock_guard run_lock(global_run_mtx);
        LOG_DEBUG("%s", "Start global gc");
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
            LOG_CRITICAL("%s", "heap overflow");
            std::exit(EXIT_FAILURE);
        }
    }
public:
    void add_to_reg(pthread_t tid, gc* new_gc) {
        std::lock_guard reg_lock(reg_mtx_);
        reg_.insert({tid, new_gc});
        tpool_.add_thread();
    }

    void erase_from_reg(pthread_t tid) {
        std::lock_guard reg_lock(reg_mtx_);
        auto itr = reg_.find(tid);
        if (itr == reg_.end()) return;
        delete itr->second;
        reg_.erase(itr);
    }

    void do_malloc(pthread_t tid, void*& dest, size_t size) {
        gc* thread_gc = get_gc(tid);
        
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
        
    }

    void do_root_marking(pthread_t tid, void* addr) {
        gc* thread_gc = get_gc(tid);

        tpool_.add_task([thread_gc](void* addr) { thread_gc->mark_root(addr); }, addr);
    }

    void do_root_unmarking(pthread_t tid, void* addr) {
        gc* thread_gc = get_gc(tid);

        tpool_.add_task([thread_gc](void* addr) { thread_gc->unmark_root(addr); }, addr);
    }

    void do_collect(pthread_t tid, int flag = THREAD_LOCAL) {
        if (flag == GLOBAL)
        {   
            LOG_DEBUG("%s", "Global collection called");
            global_run(tid);
            return;
        } else if (flag == THREAD_LOCAL)
        {
            gc* thread_gc = get_gc(tid);

            auto task_id = tpool_.add_task([thread_gc]() { thread_gc->collect(); });
            tpool_.wait(task_id);
        }
    }
};

static gc_manager manager;


void manager_malloc_wrapper(pthread_t tid, void** dest, size_t size) {
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
        LOG_CRITICAL("%s", "Sigaction: Failed to bind SIGUSR1");
        exit(EXIT_FAILURE);
    }
}

gc_handler* gc_create(pthread_t tid) {
    manager.add_to_reg(tid, new gc);
    gc_handler* handler = new gc_handler;

    handler->gc_malloc = &manager_malloc_wrapper;
    handler->gc_free = &manager_free_wrapper;
    handler->mark_root = &manager_mark_root__wrapper;
    handler->unmark_root = &manager_unmark_root_wrapper;
    handler->collect = &manager_collect_wrapper;
    stop_world_sig_init();

    return handler;
}

void gc_stop(pthread_t tid) {
    manager.erase_from_reg(tid);
}