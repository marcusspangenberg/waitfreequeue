/*
MIT License

Copyright (c) 2024 Marcus Spangenberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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

/**
 * Single header, wait-free, multiple producer, single consumer queue.
 *
 * T is the type of the elements in the queue.
 * S is the maximum number of elements in the queue. S must be a power of 2.
 */
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
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (size_t i = 0; i < S; ++i)
            {
                if (elements_[i].isUsed_.load(std::memory_order_seq_cst) == 0)
                {
                    continue;
                }
                (&elements_[i].value_)->~T();
            }
        }
#ifdef _WIN32
        _aligned_free(elements_);
#else
        free(elements_);
#endif
    }

    /**
     * @brief Pushes an item to the queue.
     *
     * @details
     * Will assert if the queue is full if asserts are enabled,
     * otherwise the behaviour is undefined. The queue should be dimensioned so that this never happens.
     *
     * Thread safe with regards to other push operations and to pop operations.
   */
    template<typename... U>
    void push(U&&... item) noexcept
    {
        const auto tail = tail_.fetch_add(1, std::memory_order_relaxed) & modValue_;
        new (&elements_[tail].value_) T(std::forward<U>(item)...);
        assert(elements_[tail].isUsed_.load(std::memory_order_acquire) == 0);
        elements_[tail].isUsed_.store(1, std::memory_order_release);
    }

    /**
     * @brief Pops an item from the queue.
     *
     * @details
     * Returns false if the queue is empty, otherwise true. item is only valid if the function returns true.
     *
     * Not thread safe with regards to other pop operations, thread safe with regards to push operations.
   */
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
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                (&elements_[head].value_)->~T();
            }
        }

        elements_[head].isUsed_.store(0, std::memory_order_relaxed);
        return true;
    }

    /**
     * @brief Checks if the queue is empty
     *
     * @details
     * Returns true if the queue is empty, otherwise false.
     *
     * Not thread safe with regards to pop operations, thread safe with regards to push operations. Regarding
     * thread safety empty() is considered a pop operation.
   */
    [[nodiscard]] bool empty() const noexcept
    {
        const auto head = head_.load(std::memory_order_relaxed) & modValue_;
        return elements_[head].isUsed_.load(std::memory_order_acquire) == 0;
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
