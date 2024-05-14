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

namespace waitfree
{

/**
 * Single header, wait-free, single producer, single consumer queue with possibility
 * to query queue size.
 *
 * T is the type of the elements in the queue.
 * S is the maximum number of elements in the queue. S must be a power of 2.
 */
template<typename T, size_t S>
class spsc_queue
{
public:
    spsc_queue()
        : size_(0),
          head_(0),
          tail_(0)
    {
        static_assert(is_power_of_two(S));
        constexpr auto alignment = std::max(alignof(T), sizeof(void*));
        constexpr auto adjusted_size = round_up_to_multiple_of(sizeof(T) * S, alignment);
#ifdef _WIN32
        auto alloc_result = _aligned_malloc(adjusted_size, alignment);
#else
        auto alloc_result = aligned_alloc(alignment, adjusted_size);
#endif
        if (!alloc_result)
        {
            throw std::bad_alloc();
        }
        memset(alloc_result, 0, adjusted_size);
        elements_ = reinterpret_cast<T*>(alloc_result);
    }

    ~spsc_queue()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            const auto size = size_.load();
            for (size_t i = 0; i < size; ++i)
            {
                const auto head = (head_ + i) & mod_value_;
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
        tail_ = (tail_ + 1) & mod_value_;
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
        head_ = (head_ + 1) & mod_value_;

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
    static constexpr size_t cacheLine_size_ = 64;
    static constexpr uint32_t mod_value_ = S - 1;

    T* elements_;
    std::atomic<size_t> size_;
    alignas(cacheLine_size_) size_t head_;
    alignas(cacheLine_size_) size_t tail_;

    static constexpr bool is_power_of_two(const size_t size)
    {
        return (size & (size - 1)) == 0;
    }

    static constexpr size_t round_up_to_multiple_of(const size_t size, const size_t multiple)
    {
        const auto remaining = size % multiple;
        const auto adjusted_size = remaining == 0 ? size : size + (multiple - remaining);
        return adjusted_size;
    }
};

}// namespace waitfree
