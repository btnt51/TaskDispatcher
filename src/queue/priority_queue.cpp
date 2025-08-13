#include "queue/priority_queue.hpp"
#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"

namespace dispatcher::queue {

PriorityQueue::PriorityQueue(const std::map<TaskPriority, QueueOptions> &queues) : active_{true} {
    for (const auto &[priority, settings] : queues) {
        std::shared_ptr<IQueue> queue;
        if (settings.bounded) {
            queue = std::make_shared<BoundedQueue>(settings.capacity.value_or(100));
        } else {
            queue = std::make_shared<UnboundedQueue>();
        }
        queues_[priority] = queue;
    }
}

void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    if (not queues_.contains(priority)) {
        throw unknown_priority("Trying to push an unknown priority queue");
    }
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this] { return active_; });
    if (not active_)
        return;
    queues_[priority]->push(std::move(task));
    condition_.notify_one();
}

std::optional<std::function<void()>> PriorityQueue::pop() {
    std::unique_lock lock(mutex_);

    while (active_) {
        if (queues_.contains(TaskPriority::High)) {
            if (auto t = queues_.at(TaskPriority::High)->try_pop(); t.has_value())
                return t;
        }

        if (queues_.contains(TaskPriority::Normal)) {
            if (auto t = queues_.at(TaskPriority::Normal)->try_pop(); t.has_value())
                return t;
        }
        condition_.wait(lock);
    }
    return std::nullopt;
}

void PriorityQueue::shutdown() {
    std::unique_lock lock(mutex_);
    active_ = false;
    condition_.notify_all();
}
PriorityQueue::~PriorityQueue() = default;

} // namespace dispatcher::queue