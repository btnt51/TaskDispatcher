#include "queue/bounded_queue.hpp"

namespace dispatcher::queue {

BoundedQueue::BoundedQueue(int capacity) : capacity_(capacity) {}

void BoundedQueue::push(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(lock, [this] { return queue_.size() < capacity_; });

    queue_.push(std::move(task));
    not_empty_.notify_one();
}

std::optional<std::function<void()>> BoundedQueue::try_pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }

    auto task = std::move(queue_.front());
    queue_.pop();
    not_full_.notify_one();
    return std::make_optional(std::move(task));
}
} // namespace dispatcher::queue