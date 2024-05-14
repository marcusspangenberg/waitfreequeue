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
 * Wait-free, multiple producer, single consumer queue.
 *
 * T is the type of the elements in the queue.
 * S is the maximum number of elements in the queue. S must be a power of 2.
 */
template<typename T, size_t S>
class mpsc_queue
{
public:
    mpsc_queue()
        : head_(0),
          tail_(0)
    {
        static_assert(is_power_of_two(S));
        constexpr auto alignment = std::max(alignof(T), sizeof(void*));
        constexpr auto adjusted_size = round_up_to_multiple_of(sizeof(element) * S, alignment);
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
        elements_ = reinterpret_cast<element*>(alloc_result);
    }

    ~mpsc_queue()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (size_t i = 0; i < S; ++i)
            {
                if (elements_[i].is_used_.load(std::memory_order_seq_cst) == 0)
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
        const auto tail = tail_.fetch_add(1, std::memory_order_relaxed) & mod_value_;
        new (&elements_[tail].value_) T(std::forward<U>(item)...);
        assert(elements_[tail].is_used_.load(std::memory_order_acquire) == 0);
        elements_[tail].is_used_.store(1, std::memory_order_release);
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
        const auto head = head_.fetch_add(1, std::memory_order_relaxed) & mod_value_;
        if (elements_[head].is_used_.load(std::memory_order_acquire) == 0)
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

        elements_[head].is_used_.store(0, std::memory_order_relaxed);
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
        const auto head = head_.load(std::memory_order_relaxed) & mod_value_;
        return elements_[head].is_used_.load(std::memory_order_acquire) == 0;
    }

private:
    static constexpr size_t cache_line_size_ = 64;
    static constexpr uint32_t mod_value_ = S - 1;

    struct element
    {
        alignas(cache_line_size_) T value_;
        std::atomic<uint_fast32_t> is_used_;
    };

    alignas(cache_line_size_) element* elements_;
    alignas(cache_line_size_) std::atomic<uint_fast32_t> head_;
    alignas(cache_line_size_) std::atomic<uint_fast32_t> tail_;

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
