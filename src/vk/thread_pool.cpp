#include "dh/vk/thread_pool.hpp"

namespace dh::vk {

ThreadPool::ThreadPool(uint32_t n_threads) {
    if (n_threads == 0) n_threads = 1;
    workers_.reserve(n_threads);
    for (uint32_t i = 0; i < n_threads; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::submit(std::function<void()> job) {
    pending_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        jobs_.push(std::move(job));
    }
    cv_.notify_one();
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (stop_ && jobs_.empty()) return;
            job = std::move(jobs_.front());
            jobs_.pop();
        }
        job();
        pending_.fetch_sub(1);
    }
}

} // namespace dh::vk