#pragma once
#include "queue.hpp"
#include "types.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace dispatcher::queue {

struct unknown_priority   : std::runtime_error { using std::runtime_error::runtime_error; };
class PriorityQueue {
    std::map<TaskPriority, std::shared_ptr<IQueue>> queues_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool active_{false};

public:
    explicit PriorityQueue(const std::map<TaskPriority, QueueOptions>& queues);

    void push(TaskPriority priority, std::function<void()> task);
    std::optional<std::function<void()>> pop();

    void shutdown();

    ~PriorityQueue();
};

}  // namespace dispatcher::queue