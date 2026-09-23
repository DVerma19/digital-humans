#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace dh::vk {

class ThreadPool {
public:
    explicit ThreadPool(uint32_t n_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> job);
    uint32_t pending() const { return pending_.load(); }
    uint32_t thread_count() const { return static_cast<uint32_t>(workers_.size()); }

private:
    void worker_loop();

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> jobs_;
    mutable std::mutex                mtx_;
    std::condition_variable           cv_;
    std::atomic<uint32_t>             pending_{0};
    bool                              stop_ = false;
};

} // namespace dh::vk