#pragma once

#include <atomic>
#ifdef _WIN32
#include <malloc.h>
#endif
#include <cassert>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

template<typename T, size_t S>
class WaitFreeMPSCQueue
{
public:
    WaitFreeMPSCQueue()
        : head_(0),
          tail_(0)
    {
        static_assert(isPowerOfTwo(S));
        const auto allocSize = sizeof(element) * S;
#ifdef _WIN32
        auto allocResult = _aligned_malloc(allocSize, alignof(element));
#else
        auto allocResult = aligned_alloc(alignof(element), allocSize);
#endif
        if (!allocResult)
        {
            throw std::bad_alloc();
        }
        memset(allocResult, 0, allocSize);
        elements_ = reinterpret_cast<element*>(allocResult);
    }

    ~WaitFreeMPSCQueue()
    {
        free(elements_);
    }

    template<typename U>
    void push(U&& item) noexcept
    {
        const auto tail = tail_.fetch_add(1, std::memory_order_relaxed) & modValue_;
        elements_[tail].value_ = std::forward<U>(item);
        assert(elements_[tail].isUsed_.load(std::memory_order_acquire) == 0);
        elements_[tail].isUsed_.store(1, std::memory_order_release);
    }

    bool pop(T& item) noexcept
    {
        const auto head = head_.fetch_add(1, std::memory_order_relaxed) & modValue_;
        if (elements_[head].isUsed_.load(std::memory_order_acquire) == 0)
        {
            head_.fetch_sub(1, std::memory_order_relaxed);
            return false;
        }

        if constexpr (std::is_move_assignable_v<T>)
        {
            item = std::move(elements_[head].value_);
        }
        else
        {
            item = elements_[head].value_;
        }

        elements_[head].isUsed_.store(0, std::memory_order_relaxed);
        return true;
    }

private:
    static constexpr size_t cacheLineSize_ = 64;
    static constexpr uint32_t modValue_ = S - 1;

    struct element {
        alignas(cacheLineSize_) T value_;
        std::atomic<uint_fast32_t> isUsed_;
    };

    alignas(cacheLineSize_) element* elements_;
    alignas(cacheLineSize_) std::atomic<uint_fast32_t> head_;
    alignas(cacheLineSize_) std::atomic<uint_fast32_t> tail_;

    static constexpr bool isPowerOfTwo(const size_t size)
    {
        return (size & (size - 1)) == 0;
    }
};
