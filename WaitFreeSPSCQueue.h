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
#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

/**
 * Single header, wait-free, single producer, single consumer queue with possibility
 * to query queue size.
 *
 * T is the type of the elements in the queue.
 * S is the maximum number of elements in the queue. S must be a power of 2.
 */
template<typename T, size_t S>
class WaitFreeSPSCQueue
{
public:
    WaitFreeSPSCQueue()
        : size_(0),
          head_(0),
          tail_(0)
    {
        static_assert(isPowerOfTwo(S));
        constexpr auto alignment = std::max(alignof(T), sizeof(void*));
        constexpr auto adjustedSize = roundUpToMultipleOf(sizeof(T) * S, alignment);
#ifdef _WIN32
        auto allocResult = _aligned_malloc(adjustedSize, alignment);
#else
        auto allocResult = aligned_alloc(alignment, adjustedSize);
#endif
        if (!allocResult)
        {
            throw std::bad_alloc();
        }
        memset(allocResult, 0, adjustedSize);
        elements_ = reinterpret_cast<T*>(allocResult);
    }

    ~WaitFreeSPSCQueue()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            const auto size = size_.load();
            for (size_t i = 0; i < size; ++i)
            {
                const auto head = (head_ + i) & modValue_;
                (&elements_[head])->~T();
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
     * Not thread safe with regards to other push operations, thread safe with regards to pop operations.
   */
    template<typename... U>
    void push(U&&... item) noexcept
    {
        const auto tail = tail_;
        tail_ = (tail_ + 1) & modValue_;
        new (&elements_[tail]) T(std::forward<U>(item)...);
        [[maybe_unused]] const auto oldValue = size_.fetch_add(1, std::memory_order_acq_rel);
        assert(oldValue < S);
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
        if (size_.load(std::memory_order_acquire) == 0)
        {
            return false;
        }

        const auto head = head_;
        head_ = (head_ + 1) & modValue_;

        if constexpr (std::is_move_assignable_v<T>)
        {
            item = std::move(elements_[head]);
        }
        else
        {
            item = elements_[head];
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                (&elements_[head])->~T();
            }
        }

        size_.fetch_sub(1, std::memory_order_acq_rel);
        return true;
    }

    /**
     * @brief Get the current size of the queue.
     *
     * Thread safe with regards to push and pop operations.
   */
    [[nodiscard]] size_t size() const noexcept
    {
        return size_.load(std::memory_order_acquire);
    }

private:
    static constexpr size_t cacheLineSize_ = 64;
    static constexpr uint32_t modValue_ = S - 1;

    T* elements_;
    std::atomic<size_t> size_;
    alignas(cacheLineSize_) size_t head_;
    alignas(cacheLineSize_) size_t tail_;

    static constexpr bool isPowerOfTwo(const size_t size)
    {
        return (size & (size - 1)) == 0;
    }

    static constexpr size_t roundUpToMultipleOf(const size_t size, const size_t multiple)
    {
        const auto remaining = size % multiple;
        const auto adjustedSize = remaining == 0 ? size : size + (multiple - remaining);
        return adjustedSize;
    }
};
