#pragma once
#include "queue/queue.hpp"

#include <condition_variable>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<std::function<void()>> queue_;
    int capacity_;
public:
    explicit BoundedQueue(int capacity);

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~BoundedQueue() override = default;
};

}  // namespace dispatcher::queue