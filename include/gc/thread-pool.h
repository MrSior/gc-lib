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

class thread_pool {
public:
    thread_pool() = default;

    explicit thread_pool(int16_t n_threads) {
        threads_.reserve(n_threads);
        for (size_t i = 0; i < n_threads; i++)
        {
            threads_.emplace_back(&thread_pool::run, this);
        }
    }

    template <typename Func, typename ...Args>
    int64_t add_task(const Func& task_func, Args&&... args) {
        int64_t task_idx = last_idx_.fetch_add(1);
        
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

    void wait_all(int64_t task_id) {
        std::unique_lock ct_lock(ct_mtx_);
        ct_cv_.wait(ct_lock, [this, task_id]() -> bool {
            return completed_tasks_idx_.find(task_id) != completed_tasks_idx_.end();
        });
    }

    void wait_all() {
        std::unique_lock q_lock(q_mtx_);
        ct_cv_.wait(q_lock, [this]() -> bool {
            std::lock_guard task_lock(ct_mtx_);
            return queue_.empty() && completed_tasks_idx_.size() == last_idx_.load();
        });
    }

    void add_thread() {
        std::lock_guard thread_lock(threads_mtx_);
        threads_.emplace_back(&thread_pool::run, this);
    }

    void remove_thread() {
        // TODO: write sth
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
};