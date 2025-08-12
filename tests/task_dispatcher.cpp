#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>
#include <map>

#include "task_dispatcher.hpp"
#include "queue/priority_queue.hpp"

using namespace std::chrono_literals;
using dispatcher::TaskPriority;
using dispatcher::TaskDispatcher;
using dispatcher::queue::QueueOptions;
using dispatcher::queue::unknown_priority;

static std::map<TaskPriority, QueueOptions> make_default_config() {
    return {
        { TaskPriority::High,   QueueOptions{ .bounded = false, .capacity = std::nullopt } },
        { TaskPriority::Normal, QueueOptions{ .bounded = true,  .capacity = 128 } }
    };
}

TEST(TaskDispatcher, ExecutesScheduledTasks) {
    TaskDispatcher d{2, make_default_config()};
    constexpr int N = 50;
    std::latch done{N};
    std::atomic<int> executed{0};

    for (int i = 0; i < N; ++i) {
        d.schedule(TaskPriority::Normal, [&]{
            executed.fetch_add(1, std::memory_order_relaxed);
            done.count_down();
        });
    }

    done.wait();
    EXPECT_EQ(executed.load(std::memory_order_acquire), N);
}

TEST(TaskDispatcher, HighPriorityBeatsNormalWhenBothQueued) {
    TaskDispatcher d{1, make_default_config()};

    std::latch gate{1};
    d.schedule(TaskPriority::High, [&]{ gate.wait(); });

    std::vector<char> order;
    std::mutex mx;

    d.schedule(TaskPriority::Normal, [&]{ std::scoped_lock lk(mx); order.push_back('N'); });
    d.schedule(TaskPriority::High,   [&]{ std::scoped_lock lk(mx); order.push_back('H'); });

    gate.count_down();

    std::latch done{2};
    d.schedule(TaskPriority::High, [&]{ done.count_down(); });
    d.schedule(TaskPriority::High, [&]{ done.count_down(); });
    done.wait();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 'H');
    EXPECT_EQ(order[1], 'N');
}

TEST(TaskDispatcher, FifoWithinSamePriorityNormal) {
    TaskDispatcher d{1, make_default_config()};

    std::vector<int> seq;
    std::mutex mx;
    std::latch done{3};

    d.schedule(TaskPriority::Normal, [&]{ { std::scoped_lock lk(mx); seq.push_back(1); } done.count_down(); });
    d.schedule(TaskPriority::Normal, [&]{ { std::scoped_lock lk(mx); seq.push_back(2); } done.count_down(); });
    d.schedule(TaskPriority::Normal, [&]{ { std::scoped_lock lk(mx); seq.push_back(3); } done.count_down(); });

    done.wait();

    ASSERT_EQ(seq.size(), 3u);
    EXPECT_EQ(seq[0], 1);
    EXPECT_EQ(seq[1], 2);
    EXPECT_EQ(seq[2], 3);
}

TEST(TaskDispatcher, ParallelismIsBoundedByThreadCount) {
    constexpr int THREADS = 2;
    TaskDispatcher d{THREADS, make_default_config()};

    std::atomic<int> current{0};
    std::atomic<int> peak{0};
    std::latch start{1};
    std::latch all_done{4};

    auto long_task = [&]{
        start.wait();
        int now = current.fetch_add(1, std::memory_order_acq_rel) + 1;
        int p = peak.load(std::memory_order_relaxed);
        while (now > p && !peak.compare_exchange_weak(p, now,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed)) {}
        std::this_thread::sleep_for(150ms);
        current.fetch_sub(1, std::memory_order_acq_rel);
        all_done.count_down();
    };

    for (int i = 0; i < 4; ++i)
        d.schedule(TaskPriority::High, long_task);

    start.count_down();
    all_done.wait();

    EXPECT_LE(peak.load(std::memory_order_acquire), THREADS);
    EXPECT_EQ(current.load(std::memory_order_acquire), 0);
}

TEST(TaskDispatcher, ScheduleUnknownPriorityThrows) {
    TaskDispatcher d{
        1,
        { { TaskPriority::Normal, QueueOptions{ .bounded = true, .capacity = 8 } } }
    };

    EXPECT_THROW(
        d.schedule(TaskPriority::High, []{}),
        unknown_priority
    );
}