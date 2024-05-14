#include "spsc_queue.h"
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

TEST(test_spsc_queue, size)
{
    const size_t total_elements = num_elements * 2;
    auto queue = std::make_unique<waitfree::spsc_queue<uint64_t, total_elements>>();

    for (uint64_t i = 0; i < num_elements; ++i)
    {
        const auto value = make_value(0, 0, i);
        queue->push(value);
    }
    EXPECT_NE(0, queue->size());

    for (uint64_t i = 0; i < num_elements; ++i)
    {
        uint64_t result;
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
    }
    EXPECT_EQ(0, queue->size());

    {
        const auto value = make_value(0, 0, 0);
        queue->push(value);
    }
    EXPECT_NE(0, queue->size());

    {
        uint64_t result;
        const auto pop_result = queue->pop(result);
        EXPECT_TRUE(pop_result);
    }
    EXPECT_EQ(0, queue->size());
}

TEST(test_spsc_queue, multi_thread_push_pop_correctness)
{
    const size_t total_elements = num_elements * num_iterations * 2;
    auto queue = std::make_unique<waitfree::spsc_queue<uint64_t, total_elements>>();
    sync_barrier<2> sync_point;

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

    size_t count0 = 0;
    std::unordered_set<uint64_t> pop_values_1;
    auto thread_0 = std::make_unique<std::thread>([&]() {
        sync_point.arrive(0);
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                uint64_t result;
                const auto pop_result = queue->pop(result);
                EXPECT_TRUE(pop_result);
                if (pop_result)
                {
                    --count0;
                    pop_values_1.insert(result);
                }
            }
        }
    });

    size_t count1 = 0;
    std::unordered_set<uint64_t> pushValues2;
    auto thread_1 = std::make_unique<std::thread>([&]() {
        sync_point.arrive(1);
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                const auto value = make_value(2, iteration, i);
                queue->push(value);
                pushValues2.insert(value);
                ++count1;
            }
        }
    });

    sync_point.run();
    thread_0->join();
    thread_1->join();

    count += count0 + count1;
    std::unordered_set<uint64_t> pop_values;
    for (size_t iteration = 0; iteration < num_iterations; ++iteration)
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

    push_values.insert(pushValues2.begin(), pushValues2.end());
    pop_values.insert(pop_values_1.begin(), pop_values_1.end());

    EXPECT_EQ(0, count);
    EXPECT_EQ(total_elements, push_values.size());
    EXPECT_EQ(total_elements, pop_values.size());
    EXPECT_EQ(0, queue->size());
}

TEST(test_spsc_queue, multi_thread_push_pop_correctness_pop_can_fail)
{
    const size_t total_elements = num_elements * num_iterations;
    auto queue = std::make_unique<waitfree::spsc_queue<uint64_t, total_elements * 4>>();

    std::unordered_set<uint64_t> pop_values;
    auto thread_0 = std::make_unique<std::thread>([&queue, &pop_values]() {
        size_t pop_count = 0;
        while (pop_count != num_elements * num_iterations)
        {
            uint64_t result;
            if (queue->pop(result))
            {
                pop_values.insert(result);
                ++pop_count;
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

TEST(test_spsc_queue, push_performance)
{
    auto queue = std::make_unique<waitfree::spsc_queue<uint64_t, num_elements * num_iterations>>();

    {
        scoped_stats_average<num_iterations> stats("test_spsc_queue::pushPerformance");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.get_ms());
        }
    }
}

TEST(test_spsc_queue, multi_thread_push_pop_performance)
{
    auto queue = std::make_unique<waitfree::spsc_queue<uint64_t, num_elements * num_iterations>>();
    sync_barrier<2> sync_point;

    auto thread_0 = std::make_unique<std::thread>([&]() {
        sync_point.arrive(0);
        scoped_stats_average<num_iterations> stats("test_spsc_queue::multiThreadPushPopPerformance pop");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                uint64_t result;
                queue->pop(result);
            }
            stats.push(timer.get_ms());
        }
    });

    auto thread_1 = std::make_unique<std::thread>([&]() {
        sync_point.arrive(1);
        scoped_stats_average<num_iterations> stats("test_spsc_queue::multiThreadPushPopPerformance push");
        for (size_t iteration = 0; iteration < num_iterations; ++iteration)
        {
            scoped_timer timer;
            for (uint64_t i = 0; i < num_elements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.get_ms());
        }
    });

    sync_point.run();
    thread_0->join();
    thread_1->join();
}

TEST(test_spsc_queue, pop_performance)
{
    auto queue = std::make_unique<waitfree::spsc_queue<uint64_t, num_elements * num_iterations>>();

    scoped_stats_average<num_iterations> stats("test_spsc_queue::popPerformance");
    for (size_t iteration = 0; iteration < num_iterations; ++iteration)
    {
        for (uint64_t i = 0; i < num_elements; ++i)
        {
            queue->push(i);
        }

        {
            scoped_timer timer;
            for (uint32_t i = 0; i < num_elements; ++i)
            {
                uint64_t result;
                const auto pop_result = queue->pop(result);
                EXPECT_TRUE(pop_result);
            }
            stats.push(timer.get_ms());
        }
    }
}

TEST(test_spsc_queue, pop_non_movable_with_non_trivial_destructor)
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
    auto queue = std::make_unique<waitfree::spsc_queue<element, 16>>();

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

TEST(test_spsc_queue, pop_non_movable_with_trivial_destructor)
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
    auto queue = std::make_unique<waitfree::spsc_queue<element, 16>>();

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
