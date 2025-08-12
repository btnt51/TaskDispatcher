#include "task_dispatcher.hpp"

#include <iostream>
#include <print>

namespace dispatcher {

TaskDispatcher::TaskDispatcher(size_t thread_count, const std::map<TaskPriority, queue::QueueOptions> &queues)
: queue_(std::make_shared<queue::PriorityQueue>(queues)), thread_pool_(queue_, thread_count) {}

void TaskDispatcher::schedule(TaskPriority priority, std::function<void()> task) {
    try {
        queue_->push(priority, std::move(task));
    } catch (...) {
        throw;
    }
}
TaskDispatcher::~TaskDispatcher() = default;
} // namespace dispatcher