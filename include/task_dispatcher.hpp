#pragma once

#include <memory>

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

namespace dispatcher {

class TaskDispatcher {
    std::shared_ptr<queue::PriorityQueue> queue_;
    thread_pool::ThreadPool thread_pool_;
public:
    TaskDispatcher(size_t thread_count = std::thread::hardware_concurrency(), const std::map<TaskPriority,
        queue::QueueOptions> &queues = {{TaskPriority::High, {true, 1000}}, {TaskPriority::Normal, {false}}});

    void schedule(TaskPriority priority, std::function<void()> task);
    ~TaskDispatcher();
};

}  // namespace dispatcher