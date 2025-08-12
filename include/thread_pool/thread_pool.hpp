#pragma once
#include "queue/priority_queue.hpp"

#include <thread>
#include <vector>

namespace dispatcher::thread_pool {

class ThreadPool {
public:
    explicit ThreadPool(std::shared_ptr<queue::PriorityQueue> queue, size_t num_threads = std::thread::hardware_concurrency());

    ~ThreadPool();

private:
    void Worker();
    std::vector<std::jthread> threads;
    std::shared_ptr<queue::PriorityQueue> queue_;
};

} // namespace dispatcher::thread_pool
