#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include <vector>

#include "queue/bounded_queue.hpp"

#include <latch>

using dispatcher::queue::BoundedQueue;
using namespace std::chrono_literals;

TEST(BoundedQueue, FifoOrder) {
    BoundedQueue q{3};
    std::vector<int> seq;

    q.push([&]{ seq.push_back(1); });
    q.push([&]{ seq.push_back(2); });
    q.push([&]{ seq.push_back(3); });

    for (int i = 1; i <= 3; ++i) {
        auto t = q.try_pop();
        ASSERT_TRUE(t.has_value());
        (*t)();
        ASSERT_EQ(seq.back(), i);
    }
    EXPECT_EQ(seq, (std::vector<int>{1,2,3}));
}

TEST(BoundedQueue, TryPopOnEmptyReturnsNullopt) {
    BoundedQueue q{1};
    auto t = q.try_pop();
    EXPECT_FALSE(t.has_value());
}
TEST(BoundedQueue, PushBlocksWhenFullAndUnblocksAfterPop) {
    BoundedQueue q{1};

    std::atomic<bool> enqueued{false};
    std::atomic<bool> executed{false};

    q.push([]{});

    std::jthread prod([&]{
        q.push([&]{ executed.store(true); });
        enqueued.store(true);
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(enqueued.load());

    auto t1 = q.try_pop();
    ASSERT_TRUE(t1.has_value());
    (*t1)();

    for (int i = 0; i < 50 && !enqueued.load(); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(enqueued.load());

    auto t2 = q.try_pop();
    ASSERT_TRUE(t2.has_value());
    (*t2)();

    EXPECT_TRUE(executed.load());
}

TEST(BoundedQueue, CapacityExactlyRespected) {
    BoundedQueue q{2};


    q.push([]{});
    q.push([]{});

    std::atomic<bool> enqueued{false};
    std::atomic<bool> executed{false};

    std::jthread prod([&]{
        q.push([&]{ executed.store(true, std::memory_order_release); });
        enqueued.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(enqueued.load(std::memory_order_acquire));

    if (auto t = q.try_pop()) (*t)();

    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!enqueued.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }
    EXPECT_TRUE(enqueued.load(std::memory_order_acquire));

    while (!executed.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        if (auto t = q.try_pop()) {
            (*t)();
        } else {
            std::this_thread::sleep_for(1ms);
        }
    }

    EXPECT_TRUE(executed.load(std::memory_order_acquire));
}

TEST(BoundedQueue, ConcurrentProducersConsumersBasic) {
    BoundedQueue q{4};

    constexpr int N1 = 100;
    constexpr int N2 = 100;
    constexpr int TARGET = N1 + N2;

    std::atomic<int> executed{0};
    std::latch done{TARGET};

    auto make_task = [&] {
        return [&] {
            executed.fetch_add(1, std::memory_order_relaxed);
            done.count_down();
        };
    };

    std::jthread p1([&]{
        for (int i = 0; i < N1; ++i) q.push(make_task());
    });
    std::jthread p2([&]{
        for (int i = 0; i < N2; ++i) q.push(make_task());
    });

    std::jthread c1([&]{
        while (!done.try_wait()) {
            if (auto t = q.try_pop()) (*t)();
            else std::this_thread::sleep_for(1ms);
        }
    });

    std::jthread c2([&]{
        while (!done.try_wait()) {
            if (auto t = q.try_pop()) (*t)();
            else std::this_thread::sleep_for(1ms);
        }
    });

    done.wait();

    std::this_thread::sleep_for(10ms);

    EXPECT_EQ(executed.load(std::memory_order_acquire), TARGET);
}