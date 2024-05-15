#include "mpsc_queue.h"
#include "test/test_helpers.h"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_set>

namespace
{

constexpr size_t num_elements = 65536;
constexpr size_t num_iterations = 4;

constexpr uint64_t make_value(const uint64_t thread_id, const uint64_t iteration, const uint64_t element_id)
{
    return (thread_id << 32) | (iteration << 16) | element_id;
}

}// namespace

TEST(test_mpsc_queue, is_empty)
{
    const size_t total_elements = num_elements * 2;
    auto queue = std::make_unique<waitfree::mpsc_queue<uint64_t, total_elements>>();

    for (uint64_t i = 0; i < num_elements; ++i)
    {
        const auto value = make_value(0, 0, i);
        queue->push(value);
    }
    EXPECT_FALSE(queue->empty());

    for (uint64_t i = 0; i < num_elements; ++i)
    {
        uint64_t result;
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
    }
    EXPECT_TRUE(queue->empty());

    {
        const auto value = make_value(0, 0, 0);
        queue->push(value);
    }
    EXPECT_FALSE(queue->empty());

    {
        uint64_t result;
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
    }
    EXPECT_TRUE(queue->empty());
}

TEST(test_mpsc_queue, multi_thread_push_pop_correctness)
{
    const size_t total_elements = num_elements * num_iterations * 4;
    auto queue = std::make_unique<waitfree::mpsc_queue<uint64_t, total_elements>>();
    sync_barrier<3> sync_point;

    size_t count = 0;
    std::unordered_set<uint64_t> push_values;
    for (size_t iteration = 0; iteration < num_iterations; ++iteration)
    {
        for (uint64_t i = 0; i < num_elements; ++i)
        {
            const auto value = make_value(0, iteration, i);
            queue->push(value);
            push_values.insert(value);
            ++count;
        }
    }

    size_t count_0 = 0;
    std::unordered_set<uint64_t> push_values_1;
    std::unordered_set<uint64_t> pop_values_1;
    auto thread_0 = std::make_unique<std::thread>([&queue, &sync_point, &count_0, &push_values_1, &pop_values_1]() {
        sync_point.arrive(0);
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                const auto value = make_value(1, iteration, i);
                queue->push(value);
                push_values_1.insert(value);
                ++count_0;

                uint64_t result;
                const auto pop_result = queue->pop(result);
                EXPECT_TRUE(pop_result);
                if (pop_result)
                {
                    --count_0;
                    pop_values_1.insert(result);
                }
            }
        }
    });

    size_t count_1 = 0;
    std::unordered_set<uint64_t> push_values_2;
    auto thread_1 = std::make_unique<std::thread>([&queue, &sync_point, &count_1, &push_values_2]() {
        sync_point.arrive(1);
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                const auto value = make_value(2, iteration, i);
                queue->push(value);
                push_values_2.insert(value);
                ++count_1;
            }
        }
    });

    size_t count_2 = 0;
    std::unordered_set<uint64_t> push_values3;
    auto thread_2 = std::make_unique<std::thread>([&queue, &sync_point, &count_2, &push_values3]() {
        sync_point.arrive(2);
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                const auto value = make_value(3, iteration, i);
                queue->push(value);
                push_values3.insert(value);
                ++count_2;
            }
        }
    });

    sync_point.run();
    thread_0->join();
    thread_1->join();
    thread_2->join();

    count += count_0 + count_1 + count_2;
    std::unordered_set<uint64_t> pop_values;
    for (size_t iteration = 0; iteration < num_iterations * 3; ++iteration)
    {
        for (uint64_t i = 0; i < num_elements; ++i)
        {
            uint64_t result;
            const auto pop_result = queue->pop(result);
            EXPECT_TRUE(pop_result);
            if (pop_result)
            {
                --count;
                pop_values.insert(result);
            }
        }
    }

    push_values.insert(push_values_1.begin(), push_values_1.end());
    push_values.insert(push_values_2.begin(), push_values_2.end());
    push_values.insert(push_values3.begin(), push_values3.end());
    pop_values.insert(pop_values_1.begin(), pop_values_1.end());

    EXPECT_EQ(0, count);
    EXPECT_EQ(total_elements, push_values.size());
    EXPECT_EQ(total_elements, pop_values.size());
    EXPECT_TRUE(queue->empty());
}

TEST(test_mpsc_queue, multi_thread_push_pop_correctness_pop_can_fail)
{
    const size_t total_elements = num_elements * num_iterations;
    auto queue = std::make_unique<waitfree::mpsc_queue<uint64_t, total_elements * 4>>();

    std::unordered_set<uint64_t> pop_values;
    auto thread_0 = std::make_unique<std::thread>([&queue, &pop_values]() {
        size_t popCount = 0;
        while (popCount != num_elements * num_iterations)
        {
            uint64_t result;
            if (queue->pop(result))
            {
                pop_values.insert(result);
                ++popCount;
            }
        }
    });

    std::unordered_set<uint64_t> push_values;
    auto thread_1 = std::make_unique<std::thread>([&queue, &push_values]() {
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                const auto value = make_value(1, iteration, i);
                queue->push(value);
                push_values.insert(value);
                std::this_thread::yield();
            }
        }
    });

    thread_0->join();
    thread_1->join();

    for (const auto& value : push_values)
    {
        EXPECT_NE(pop_values.find(value), pop_values.end());
    }

    EXPECT_EQ(total_elements, push_values.size());
    EXPECT_EQ(total_elements, pop_values.size());
}

TEST(test_mpsc_queue, multi_thread_push_performance)
{
    auto queue = std::make_unique<waitfree::mpsc_queue<uint32_t, num_elements * num_iterations * 2>>();
    sync_barrier<2> sync_point;

    auto pop_thread_0 = std::make_unique<std::thread>([&queue, &sync_point]() {
        sync_point.arrive(0);
        scoped_stats_average<num_iterations> stats("test_mpsc_queue::multiThreadPushPerformance 0");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint32_t i = 0; i < num_elements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.get_ms());
        }
    });

    auto pop_thread_1 = std::make_unique<std::thread>([&queue, &sync_point]() {
        sync_point.arrive(1);
        scoped_stats_average<num_iterations> stats("test_mpsc_queue::multiThreadPushPerformance 1");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint32_t i = 0; i < num_elements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.get_ms());
        }
    });

    sync_point.run();
    pop_thread_0->join();
    pop_thread_1->join();
}

TEST(test_mpsc_queue, multi_thread_push_pop_performance)
{
    auto queue = std::make_unique<waitfree::mpsc_queue<uint32_t, num_elements * num_iterations * 2>>();
    sync_barrier<2> sync_point;

    auto pop_thread_0 = std::make_unique<std::thread>([&queue, &sync_point]() {
        sync_point.arrive(0);
        scoped_stats_average<num_iterations> stats("test_mpsc_queue::multiThreadPushPopPerformance push pop");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint32_t i = 0; i < num_elements; ++i)
            {
                queue->push(i);
                uint32_t result;
                queue->pop(result);
            }
            stats.push(timer.get_ms());
        }
    });

    auto pop_thread_1 = std::make_unique<std::thread>([&queue, &sync_point]() {
        sync_point.arrive(1);
        scoped_stats_average<num_iterations> stats("test_mpsc_queue::multiThreadPushPopPerformance push");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint32_t i = 0; i < num_elements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.get_ms());
        }
    });

    sync_point.run();
    pop_thread_0->join();
    pop_thread_1->join();
}

TEST(test_mpsc_queue, pop_performance)
{
    auto queue = std::make_unique<waitfree::mpsc_queue<uint32_t, num_elements * num_iterations>>();

    scoped_stats_average<num_iterations> stats("test_mpsc_queue::popPerformance");
    for (size_t iteration = 0; iteration < num_iterations; ++iteration)
    {
        for (uint32_t i = 0; i < num_elements; ++i)
        {
            queue->push(i);
        }

        {
            scoped_timer timer;
            for (uint32_t i = 0; i < num_elements; ++i)
            {
                uint32_t result;
                const auto pop_result = queue->pop(result);
                EXPECT_TRUE(pop_result);
            }
            stats.push(timer.get_ms());
        }
    }
}

TEST(test_mpsc_queue, pop_non_movable_with_non_trivial_destructor)
{
    uint32_t count = 0;

    struct element
    {
        element() = delete;

        explicit element(const uint32_t a, uint32_t* count)
            : a_(a),
              count_(count)
        {
            (*count_)++;
        }

        element(const element& o)
            : a_(o.a_),
              count_(o.count_)
        {
            (*count_)++;
        }
        element(element&& o) noexcept = delete;

        ~element()
        {
            a_ = 0;
            (*count_)--;
        }

        element& operator=(element&& o) = delete;
        element& operator=(const element& o)
        {
            if (this == &o)
            {
                return *this;
            }

            a_ = o.a_;
            count_ = o.count_;
            return *this;
        }

        uint32_t a_;
        uint32_t* count_;
    };
    auto queue = std::make_unique<waitfree::mpsc_queue<element, 16>>();

    queue->push(1, &count);
    queue->push(2, &count);

    {
        element result(0, &count);
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
        EXPECT_EQ(1, result.a_);
    }
    EXPECT_EQ(1, count);
    {
        element result(0, &count);
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
        EXPECT_EQ(2, result.a_);
    }
    EXPECT_EQ(0, count);
}

TEST(test_mpsc_queue, pop_non_movable_with_trivial_destructor)
{
    struct element
    {
        element()
            : a_(0)
        {
        }
        explicit element(const uint32_t a)
            : a_(a)
        {
        }

        element(const element& o) = default;
        element(element&& o) noexcept = delete;

        ~element() = default;

        element& operator=(element&& o) = delete;
        element& operator=(const element& o) = default;
        uint32_t a_;
    };
    auto queue = std::make_unique<waitfree::mpsc_queue<element, 16>>();

    queue->push(1);
    queue->push(2);

    {
        element result;
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
        EXPECT_EQ(1, result.a_);
    }
    {
        element result;
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
        EXPECT_EQ(2, result.a_);
    }
}
