#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

class ScopedTimer
{
public:
    explicit ScopedTimer()
        : start_(std::chrono::high_resolution_clock::now())
    {
    }

    [[nodiscard]] double getMs() const
    {
        auto elapsed = std::chrono::high_resolution_clock::now() - start_;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point elapsed_;
};

template<size_t S>
class ScopedStatsAverage
{
public:
    explicit ScopedStatsAverage(std::string name)
        : name_(std::move(name))
    {
    }

    ~ScopedStatsAverage()
    {
        double sum = 0.0;
        for (const auto value : values_)
        {
            sum += value;
        }

        printf("%s: %f\n", name_.c_str(), sum / S);
    }

    void push(const double value)
    {
        values_[index_] = value;
        index_ = (index_ + 1) % S;
    }

private:
    size_t index_ = 0;
    std::array<double, S> values_{0.0};
    std::string name_;
};

template<size_t N>
struct SyncBarrier {
    std::atomic_flag flagsWait_[N] = {ATOMIC_FLAG_INIT};
    std::atomic_flag flagsContinue_[N] = {ATOMIC_FLAG_INIT};

    SyncBarrier()
    {
        for (size_t i = 0; i < N; ++i)
        {
            flagsWait_[i].test_and_set(std::memory_order_acquire);
            flagsContinue_[i].test_and_set(std::memory_order_acquire);
        }
    }

    void arrive(const size_t threadId)
    {
        flagsWait_[threadId].clear(std::memory_order_release);
        while (flagsContinue_[threadId].test_and_set(std::memory_order_acquire))
        {
        }
    }

    void run()
    {
        for (size_t i = 0; i < N; ++i)
        {
            while (flagsWait_[i].test_and_set(std::memory_order_acquire))
            {
            }
        }

        for (size_t i = 0; i < N; ++i)
        {
            flagsContinue_[i].clear(std::memory_order_release);
        }
    }
};
