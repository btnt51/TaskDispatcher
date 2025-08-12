#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include <vector>

#include "queue/unbounded_queue.hpp"

using dispatcher::queue::UnboundedQueue;
using namespace std::chrono_literals;

namespace {
void sleep_briefly(auto d) { std::this_thread::sleep_for(d); }

std::optional<std::function<void()>> wait_for_task(
    UnboundedQueue& q, std::chrono::milliseconds timeout, std::chrono::milliseconds step = 2ms)
{
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (auto t = q.try_pop(); t.has_value()) return t;
        sleep_briefly(step);
    }
    return std::nullopt;
}
} // namespace

TEST(UnboundedQueue, FifoOrder) {
    UnboundedQueue q;

    std::vector<int> order;
    order.reserve(4);

    auto make_task = [&](int v) { return [&, v] { order.push_back(v); }; };

    q.push(make_task(1));
    q.push(make_task(2));
    q.push(make_task(3));
    q.push(make_task(4));

    for (int i = 0; i < 4; ++i) {
        auto task = q.try_pop();
        ASSERT_TRUE(task.has_value());
        (*task)();
    }

    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
    EXPECT_EQ(order[3], 4);
}
TEST(UnboundedQueue, NonBlockingPopEmptyThenPush) {
    UnboundedQueue q;

    auto t0 = q.try_pop();
    EXPECT_FALSE(t0.has_value());

    std::atomic<bool> ran{false};
    q.push([&]{ ran = true; });

    auto t1 = wait_for_task(q, 100ms);
    ASSERT_TRUE(t1.has_value());
    (*t1)();
    EXPECT_TRUE(ran.load());
}

TEST(UnboundedQueue, SinglePushPickedByExactlyOneConsumer) {
    UnboundedQueue q;
    std::atomic<int> completed{0};

    std::atomic<bool> stop{false};
    auto consumer = [&](std::atomic<int>& hit){
        while (!stop.load(std::memory_order_relaxed)) {
            if (auto t = q.try_pop(); t.has_value()) {
                (*t)();
                hit.fetch_add(1, std::memory_order_relaxed);
                break;
            }
            sleep_briefly(2ms);
        }
    };

    std::atomic<int> c1{0}, c2{0};
    std::thread t1(consumer, std::ref(c1));
    std::thread t2(consumer, std::ref(c2));

    sleep_briefly(30ms);

    q.push([&]{ completed.fetch_add(1, std::memory_order_relaxed); });

    auto waited = 0ms;
    while (completed.load() == 0 && waited < 200ms) {
        sleep_briefly(5ms);
        waited += 5ms;
    }
    EXPECT_EQ(completed.load(), 1);

    stop = true;
    t1.join();
    t2.join();

    const int picks = c1.load() + c2.load();
    EXPECT_EQ(picks, 1);
}

TEST(UnboundedQueue, MultiplePushesPickedBySameNumberOfConsumers) {
    UnboundedQueue q;

    constexpr int WAITERS = 3;
    std::atomic<int> executed{0};
    std::atomic<int> picked{0};

    auto consumer = [&] {
        auto task = wait_for_task(q, 300ms);
        if (task) {
            picked.fetch_add(1, std::memory_order_relaxed);
            (*task)();
        }
    };

    std::vector<std::thread> consumers;
    consumers.reserve(WAITERS);
    for (auto i = 0; i < WAITERS; ++i)
        consumers.emplace_back(consumer);

    sleep_briefly(30ms);


    for (auto i = 0; i < WAITERS; ++i)
        q.push([&]{ executed.fetch_add(1, std::memory_order_relaxed); });

    for (auto& th : consumers) th.join();

    EXPECT_EQ(picked.load(), WAITERS);
    EXPECT_EQ(executed.load(), WAITERS);
}

TEST(UnboundedQueue, StressMPMC_NoDeadlock_ExactNExecuted_NonBlockingPop) {
    UnboundedQueue q;

    constexpr int N = 300;
    constexpr int PRODUCERS = 3;
    constexpr int CONSUMERS = 4;

    std::atomic<int> executed{0};
    std::atomic<bool> producers_done{false};

    std::vector<std::thread> producers;
    producers.reserve(PRODUCERS);
    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&, p] {
            const int chunk = N / PRODUCERS;
            const int start = p * chunk;
            const int end   = (p == PRODUCERS - 1) ? N : start + chunk;
            for (int i = start; i < end; ++i) {
                q.push([&] { executed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }

    // --- Consumers: крутятся пока не выполнено N задач
    std::vector<std::thread> consumers;
    consumers.reserve(CONSUMERS);
    for (int c = 0; c < CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            for (;;) {
                if (auto task = q.try_pop(); task.has_value()) {
                    (*task)();
                    if (executed.load(std::memory_order_relaxed) >= N) break;
                } else {
                    if (producers_done.load(std::memory_order_relaxed) &&
                        executed.load(std::memory_order_relaxed) >= N) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    producers_done.store(true, std::memory_order_relaxed);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    for (auto& t : consumers) {
        while (t.joinable()) {
            if (t.joinable()) {
                using namespace std::chrono_literals;
                if (std::chrono::steady_clock::now() > deadline) break;
                std::this_thread::sleep_for(5ms);
                t.join();
            }
        }
    }

    EXPECT_EQ(executed.load(), N);
}