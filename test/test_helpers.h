#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

class scoped_timer
{
public:
    explicit scoped_timer()
        : start_(std::chrono::high_resolution_clock::now())
    {
    }

    [[nodiscard]] double get_ms() const
    {
        auto elapsed = std::chrono::high_resolution_clock::now() - start_;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point elapsed_;
};

template<size_t S>
class scoped_stats_average
{
public:
    explicit scoped_stats_average(std::string name)
        : name_(std::move(name))
    {
    }

    ~scoped_stats_average()
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
struct sync_barrier
{
    std::atomic_flag flags_wait_[N] = {ATOMIC_FLAG_INIT};
    std::atomic_flag flags_continue_[N] = {ATOMIC_FLAG_INIT};

    sync_barrier()
    {
        for (size_t i = 0; i < N; ++i)
        {
            flags_wait_[i].test_and_set(std::memory_order_acquire);
            flags_continue_[i].test_and_set(std::memory_order_acquire);
        }
    }

    void arrive(const size_t threadId)
    {
        flags_wait_[threadId].clear(std::memory_order_release);
        while (flags_continue_[threadId].test_and_set(std::memory_order_acquire))
        {
        }
    }

    void run()
    {
        for (size_t i = 0; i < N; ++i)
        {
            while (flags_wait_[i].test_and_set(std::memory_order_acquire))
            {
            }
        }

        for (size_t i = 0; i < N; ++i)
        {
            flags_continue_[i].clear(std::memory_order_release);
        }
    }
};
