#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "queue/priority_queue.hpp"

using dispatcher::queue::PriorityQueue;
using dispatcher::queue::QueueOptions;
using dispatcher::TaskPriority;
using dispatcher::queue::unknown_priority;


using namespace std::chrono_literals;

inline std::map<TaskPriority, QueueOptions> make_default_config() {
    return {
            { TaskPriority::High,   QueueOptions{ .bounded = true,  .capacity = 2 } },
            { TaskPriority::Normal, QueueOptions{ .bounded = false, .capacity = std::nullopt } }
    };
}

TEST(PriorityQueue, HighPriorityBeatsNormal) {
    PriorityQueue pq{ make_default_config() };
    std::vector<int> seq;

    pq.push(TaskPriority::Normal, [&]{ seq.push_back(2); });
    pq.push(TaskPriority::High,   [&]{ seq.push_back(1); });

    auto t1 = pq.pop(); ASSERT_TRUE(t1.has_value()); (*t1)();
    auto t2 = pq.pop(); ASSERT_TRUE(t2.has_value()); (*t2)();

    ASSERT_EQ(seq.size(), 2u);
    EXPECT_EQ(seq[0], 1);
    EXPECT_EQ(seq[1], 2);
}

TEST(PriorityQueue, FifoWithinSamePriorityNormal) {
    PriorityQueue pq{ make_default_config() };
    std::vector<int> seq;

    pq.push(TaskPriority::Normal, [&]{ seq.push_back(1); });
    pq.push(TaskPriority::Normal, [&]{ seq.push_back(2); });
    pq.push(TaskPriority::Normal, [&]{ seq.push_back(3); });

    for (int i=0;i<3;++i) {
        auto t = pq.pop(); ASSERT_TRUE(t.has_value()); (*t)();
    }
    ASSERT_EQ(seq.size(), 3u);
    EXPECT_EQ(seq[0],1);
    EXPECT_EQ(seq[1],2);
    EXPECT_EQ(seq[2],3);
}

TEST(PriorityQueue, PopBlocksUntilTaskArrives) {
    PriorityQueue pq{ make_default_config() };
    std::atomic<bool> executed{false};

    std::jthread consumer([&]{
        auto t = pq.pop();
        ASSERT_TRUE(t.has_value());
        (*t)();
        executed.store(true);
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(executed.load());

    pq.push(TaskPriority::High, []{ });

    // Дать время на пробуждение
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(executed.load());
}

TEST(PriorityQueue, ShutdownUnblocksPopAndReturnsNullopt) {
    PriorityQueue pq{ make_default_config() };
    std::optional<std::function<void()>> out;

    std::jthread consumer([&]{
        out = pq.pop();
    });

    std::this_thread::sleep_for(50ms);
    pq.shutdown();

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(out.has_value());
}

TEST(PriorityQueue, PushToUnknownPriorityThrows) {
    PriorityQueue pq{ {
        { TaskPriority::Normal, QueueOptions{ .bounded=false, .capacity=std::nullopt } }
    } };

    EXPECT_THROW(
        pq.push(TaskPriority::High, []{}),
        unknown_priority
    );
}