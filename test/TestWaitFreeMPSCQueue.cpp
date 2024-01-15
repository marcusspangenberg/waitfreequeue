#include "WaitFreeMPSCQueue.h"
#include "test/TestHelpers.h"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_set>

namespace
{

constexpr size_t numElements = 65536;
constexpr size_t numIterations = 4;

constexpr uint64_t makeValue(const uint64_t threadId, const uint64_t iteration, const uint64_t elementId)
{
    return (threadId << 32) | (iteration << 16) | elementId;
}

}// namespace

TEST(TestWaitFreeQueue, isEmpty)
{
    const size_t totalElements = numElements * 2;
    auto queue = std::make_unique<WaitFreeMPSCQueue<uint64_t, totalElements>>();

    for (uint64_t i = 0; i < numElements; ++i)
    {
        const auto value = makeValue(0, 0, i);
        queue->push(value);
    }
    EXPECT_FALSE(queue->empty());

    for (uint64_t i = 0; i < numElements; ++i)
    {
        uint64_t result;
        const auto popResult = queue->pop(result);
        EXPECT_TRUE(popResult);
    }
    EXPECT_TRUE(queue->empty());

    {
        const auto value = makeValue(0, 0, 0);
        queue->push(value);
    }
    EXPECT_FALSE(queue->empty());

    {
        uint64_t result;
        const auto popResult = queue->pop(result);
        EXPECT_TRUE(popResult);
    }
    EXPECT_TRUE(queue->empty());
}

TEST(TestWaitFreeQueue, multiThreadPushPopCorrectness)
{
    const size_t totalElements = numElements * numIterations * 4;
    auto queue = std::make_unique<WaitFreeMPSCQueue<uint64_t, totalElements>>();
    SyncBarrier<3> syncPoint;

    size_t count = 0;
    std::unordered_set<uint64_t> pushValues;
    for (size_t iteration = 0; iteration < numIterations; ++iteration)
    {
        for (uint64_t i = 0; i < numElements; ++i)
        {
            const auto value = makeValue(0, iteration, i);
            queue->push(value);
            pushValues.insert(value);
            ++count;
        }
    }

    size_t count0 = 0;
    std::unordered_set<uint64_t> pushValues1;
    std::unordered_set<uint64_t> popValues1;
    auto thread0 = std::make_unique<std::thread>([&queue, &syncPoint, &count0, &pushValues1, &popValues1]() {
        syncPoint.arrive(0);
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            for (uint64_t i = 0; i < numElements; ++i)
            {
                const auto value = makeValue(1, iteration, i);
                queue->push(value);
                pushValues1.insert(value);
                ++count0;

                uint64_t result;
                const auto popResult = queue->pop(result);
                EXPECT_TRUE(popResult);
                if (popResult)
                {
                    --count0;
                    popValues1.insert(result);
                }
            }
        }
    });

    size_t count1 = 0;
    std::unordered_set<uint64_t> pushValues2;
    auto thread1 = std::make_unique<std::thread>([&queue, &syncPoint, &count1, &pushValues2]() {
        syncPoint.arrive(1);
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            for (uint64_t i = 0; i < numElements; ++i)
            {
                const auto value = makeValue(2, iteration, i);
                queue->push(value);
                pushValues2.insert(value);
                ++count1;
            }
        }
    });

    size_t count2 = 0;
    std::unordered_set<uint64_t> pushValues3;
    auto thread_2 = std::make_unique<std::thread>([&queue, &syncPoint, &count2, &pushValues3]() {
        syncPoint.arrive(2);
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            for (uint64_t i = 0; i < numElements; ++i)
            {
                const auto value = makeValue(3, iteration, i);
                queue->push(value);
                pushValues3.insert(value);
                ++count2;
            }
        }
    });

    syncPoint.run();
    thread0->join();
    thread1->join();
    thread_2->join();

    count += count0 + count1 + count2;
    std::unordered_set<uint64_t> popValues;
    for (size_t iteration = 0; iteration < numIterations * 3; ++iteration)
    {
        for (uint64_t i = 0; i < numElements; ++i)
        {
            uint64_t result;
            const auto popResult = queue->pop(result);
            EXPECT_TRUE(popResult);
            if (popResult)
            {
                --count;
                popValues.insert(result);
            }
        }
    }

    pushValues.insert(pushValues1.begin(), pushValues1.end());
    pushValues.insert(pushValues2.begin(), pushValues2.end());
    pushValues.insert(pushValues3.begin(), pushValues3.end());
    popValues.insert(popValues1.begin(), popValues1.end());

    EXPECT_EQ(0, count);
    EXPECT_EQ(totalElements, pushValues.size());
    EXPECT_EQ(totalElements, popValues.size());
    EXPECT_TRUE(queue->empty());
}

TEST(TestWaitFreeQueue, multiThreadPushPopCorrectnessPopCanFail)
{
    const size_t totalElements = numElements * numIterations;
    auto queue = std::make_unique<WaitFreeMPSCQueue<uint64_t, totalElements * 4>>();

    std::unordered_set<uint64_t> popValues;
    auto thread0 = std::make_unique<std::thread>([&queue, &popValues]() {
        size_t popCount = 0;
        while (popCount != numElements * numIterations)
        {
            uint64_t result;
            if (queue->pop(result))
            {
                popValues.insert(result);
                ++popCount;
            }
        }
    });

    std::unordered_set<uint64_t> pushValues;
    auto thread1 = std::make_unique<std::thread>([&queue, &pushValues]() {
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            for (uint64_t i = 0; i < numElements; ++i)
            {
                const auto value = makeValue(1, iteration, i);
                queue->push(value);
                pushValues.insert(value);
                std::this_thread::yield();
            }
        }
    });

    thread0->join();
    thread1->join();

    for (const auto& value : pushValues)
    {
        EXPECT_NE(popValues.find(value), popValues.end());
    }

    EXPECT_EQ(totalElements, pushValues.size());
    EXPECT_EQ(totalElements, popValues.size());
}

TEST(TestWaitFreeQueue, multiThreadPushPerformance)
{
    auto queue = std::make_unique<WaitFreeMPSCQueue<uint32_t, numElements * numIterations * 2>>();
    SyncBarrier<2> syncPoint;

    auto popThread0 = std::make_unique<std::thread>([&queue, &syncPoint]() {
        syncPoint.arrive(0);
        ScopedStatsAverage<numIterations> stats("TestWaitFreeQueue::multiThreadPushPerformance 0");
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            ScopedTimer timer;
            for (uint32_t i = 0; i < numElements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.getMs());
        }
    });

    auto popThread1 = std::make_unique<std::thread>([&queue, &syncPoint]() {
        syncPoint.arrive(1);
        ScopedStatsAverage<numIterations> stats("TestWaitFreeQueue::multiThreadPushPerformance 1");
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            ScopedTimer timer;
            for (uint32_t i = 0; i < numElements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.getMs());
        }
    });

    syncPoint.run();
    popThread0->join();
    popThread1->join();
}

TEST(TestWaitFreeQueue, multiThreadPushPopPerformance)
{
    auto queue = std::make_unique<WaitFreeMPSCQueue<uint32_t, numElements * numIterations * 2>>();
    SyncBarrier<2> syncPoint;

    auto popThread0 = std::make_unique<std::thread>([&queue, &syncPoint]() {
        syncPoint.arrive(0);
        ScopedStatsAverage<numIterations> stats("TestWaitFreeQueue::multiThreadPushPopPerformance push pop");
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            ScopedTimer timer;
            for (uint32_t i = 0; i < numElements; ++i)
            {
                queue->push(i);
                uint32_t result;
                queue->pop(result);
            }
            stats.push(timer.getMs());
        }
    });

    auto popThread1 = std::make_unique<std::thread>([&queue, &syncPoint]() {
        syncPoint.arrive(1);
        ScopedStatsAverage<numIterations> stats("TestWaitFreeQueue::multiThreadPushPopPerformance push");
        for (size_t iteration = 0; iteration < numIterations; ++iteration)
        {
            ScopedTimer timer;
            for (uint32_t i = 0; i < numElements; ++i)
            {
                queue->push(i);
            }
            stats.push(timer.getMs());
        }
    });

    syncPoint.run();
    popThread0->join();
    popThread1->join();
}

TEST(TestWaitFreeQueue, popPerformance)
{
    auto queue = std::make_unique<WaitFreeMPSCQueue<uint32_t, numElements * numIterations>>();

    ScopedStatsAverage<numIterations> stats("TestWaitFreeQueue::popPerformance");
    for (size_t iteration = 0; iteration < numIterations; ++iteration)
    {
        for (uint32_t i = 0; i < numElements; ++i)
        {
            queue->push(i);
        }

        {
            ScopedTimer timer;
            for (uint32_t i = 0; i < numElements; ++i)
            {
                uint32_t result;
                const auto popResult = queue->pop(result);
                EXPECT_TRUE(popResult);
            }
            stats.push(timer.getMs());
        }
    }
}

TEST(TestWaitFreeQueue, popNonMovableWithNonTrivialDestructor)
{
    uint32_t count = 0;

    struct Element {
        Element() = delete;

        explicit Element(const uint32_t a, uint32_t* count)
            : a_(a),
              count_(count)
        {
            (*count_)++;
        }

        Element(const Element& o)
            : a_(o.a_),
              count_(o.count_)
        {
            (*count_)++;
        }
        Element(Element&& o) noexcept = delete;

        ~Element()
        {
            a_ = 0;
            (*count_)--;
        }

        Element& operator=(Element&& o) = delete;
        Element& operator=(const Element& o)
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
    auto queue = std::make_unique<WaitFreeMPSCQueue<Element, 16>>();

    queue->push(1, &count);
    queue->push(2, &count);

    {
        Element result(0, &count);
        const auto popResult = queue->pop(result);
        EXPECT_TRUE(popResult);
        EXPECT_EQ(1, result.a_);
    }
    EXPECT_EQ(1, count);
    {
        Element result(0, &count);
        const auto popResult = queue->pop(result);
        EXPECT_TRUE(popResult);
        EXPECT_EQ(2, result.a_);
    }
    EXPECT_EQ(0, count);
}

TEST(TestWaitFreeQueue, popNonMovableWithTrivialDestructor)
{
    struct Element {
        Element()
            : a_(0)
        {
        }
        explicit Element(const uint32_t a)
            : a_(a)
        {
        }

        Element(const Element& o) = default;
        Element(Element&& o) noexcept = delete;

        ~Element() = default;

        Element& operator=(Element&& o) = delete;
        Element& operator=(const Element& o) = default;
        uint32_t a_;
    };
    auto queue = std::make_unique<WaitFreeMPSCQueue<Element, 16>>();

    queue->push(1);
    queue->push(2);

    {
        Element result;
        const auto popResult = queue->pop(result);
        EXPECT_TRUE(popResult);
        EXPECT_EQ(1, result.a_);
    }
    {
        Element result;
        const auto popResult = queue->pop(result);
        EXPECT_TRUE(popResult);
        EXPECT_EQ(2, result.a_);
    }
}
