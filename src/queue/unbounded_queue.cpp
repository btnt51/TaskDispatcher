#include "queue/unbounded_queue.hpp"
#include <functional>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

UnboundedQueue::UnboundedQueue() = default;

void UnboundedQueue::push(std::function<void()> task) {
    std::unique_lock lock(mutex_);
    queue_.push(std::move(task));
    not_empty_.notify_one();
}

std::optional<std::function<void()>> UnboundedQueue::try_pop() {
    std::unique_lock lock(mutex_);
    if (queue_.empty())
        return std::nullopt;
    auto task = std::move(queue_.front());
    queue_.pop();
    return std::make_optional(std::move(task));
}

UnboundedQueue::~UnboundedQueue() = default;

} // namespace dispatcher::queue