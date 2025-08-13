#include "thread_pool/thread_pool.hpp"

namespace dispatcher::thread_pool {

ThreadPool::ThreadPool(std::shared_ptr<queue::PriorityQueue> queue, size_t num_threads) : queue_(std::move(queue)) {
    threads.reserve(num_threads);
    for (size_t i = 0; i < num_threads; i++) {
        threads.emplace_back(&ThreadPool::Worker, this);
    }
}

ThreadPool::~ThreadPool() {
    queue_->shutdown();
    for (auto &thread : threads) {
        if (thread.joinable())
            thread.join();
    }
}

void ThreadPool::Worker() {
    while (true) {
        auto task = queue_->pop();
        if (task == std::nullopt) {
            break;
        }
        try {
            std::invoke(*task);
        } catch (std::exception &e) {
            Logger::Get().Log(std::format("While running task occured exception in thread: {}", e.what()));
        } catch (...) {
            Logger::Get().Log(std::format("While running task occured Unknown exception in thread"));
        }
    }
}

} // namespace dispatcher::thread_pool