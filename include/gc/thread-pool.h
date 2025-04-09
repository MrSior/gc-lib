#include <iostream>
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <future>
#include <unordered_set>
#include <atomic>

#include <vector>
#include <chrono>

#include "gc/log.h"

class thread_pool {
public:
    thread_pool() = default;

    explicit thread_pool(uint16_t n_threads) {
        threads_.reserve(n_threads);
        for (size_t i = 0; i < n_threads; i++)
        {
            threads_.emplace_back(&thread_pool::run, this);
        }
    }

    template <typename Func, typename ...Args>
    int64_t add_task(const Func& task_func, Args&&... args) {
        std::unique_lock add_lock(add_task_mtx_);
        add_task_cv_.wait(add_lock, [this](){
            return !block_tpool_.load();
        });

        // while (block_tpool_.load()) {}

        int64_t task_idx = last_idx_.fetch_add(1);
        
        std::lock_guard q_lock(q_mtx_);
        queue_.emplace(std::async(std::launch::deferred, task_func, args...), task_idx);
        q_cv_.notify_one();
        return task_idx;
    }

    template <typename Func, typename ...Args>
    int64_t add_priority_task(const Func& task_func, Args&&... args) {
        int64_t task_idx = last_idx_.fetch_add(1);
        
        LOG_DEBUG("Tpool q_mutex = %d", (int)check_q_mutex());
        std::lock_guard q_lock(q_mtx_);
        queue_.emplace(std::async(std::launch::deferred, task_func, args...), task_idx);
        q_cv_.notify_one();
        return task_idx;
    }

    void wait(int64_t task_id) {
        std::unique_lock ct_lock(ct_mtx_);
        ct_cv_.wait(ct_lock, [this, task_id]() -> bool {
            return completed_tasks_idx_.find(task_id) != completed_tasks_idx_.end();
        });
    }

    // wait when all tasks in thread pool will finish. Can be used only after blocking thread pool.
    void wait_all() {
        LOG_DEBUG("%s", "In waiting all");
        std::unique_lock ct_lock(ct_mtx_);
        ct_cv_.wait(ct_lock, [this]() -> bool {
            std::lock_guard q_lock(q_mtx_);
            return queue_.empty() && completed_tasks_idx_.size() == last_idx_.load();
        });
    }

    bool check_q_mutex() {
        auto res = q_mtx_.try_lock();
        q_mtx_.unlock();
        return res;
    }

    void block() {
        block_tpool_.store(true);
    }

    void unblock() {
        block_tpool_.store(false);
        add_task_cv_.notify_all();
    }

    void add_thread() {
        std::lock_guard thread_lock(threads_mtx_);
        threads_.emplace_back(&thread_pool::run, this);
    }

    void remove_thread() {
        // TODO: write sth
    }

    uint16_t get_threads_n() {
        std::lock_guard thread_lock(threads_mtx_);
        return static_cast<uint16_t>(threads_.size());
    }

    ~thread_pool() {
        quite_.store(true);
        for (size_t i = 0; i < threads_.size(); i++)
        {
            q_cv_.notify_all();
            threads_[i].join();
        }
    }

private:
    void run() {
        while (!quite_)
        {
            std::unique_lock q_lock(q_mtx_);
            q_cv_.wait(q_lock, [this]() -> bool {
                return !queue_.empty() || quite_;
            });

            if (block_tpool_.load())
            {
                LOG_DEBUG("%s", "RUN");
            }
            

            if (!queue_.empty())
            {
                auto elem = std::move(queue_.front());
                queue_.pop();
                q_lock.unlock();

                elem.first.get();

                std::lock_guard ct_lock(ct_mtx_);
                completed_tasks_idx_.insert(elem.second);

                ct_cv_.notify_all();
            }   
        }
    }

    std::queue<std::pair<std::future<void>, int64_t>> queue_;
    std::mutex q_mtx_;
    std::condition_variable q_cv_;

    std::unordered_set<int64_t> completed_tasks_idx_;
    std::mutex ct_mtx_;
    std::condition_variable ct_cv_;

    std::vector<std::thread> threads_;
    std::mutex threads_mtx_;

    std::atomic<bool> quite_ = false;
    std::atomic<int64_t> last_idx_ = 0;

    std::condition_variable add_task_cv_;
    std::mutex add_task_mtx_;
    std::atomic<bool> block_tpool_ = false;
};