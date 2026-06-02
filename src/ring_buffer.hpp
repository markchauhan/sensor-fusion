// ring_buffer.hpp
// Lock-free single-producer single-consumer (SPSC) ring buffer
//


#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>

template<typename T, std::size_t N>
class RingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
    static_assert(N >= 2, "N must be at least 2");

private:
    static constexpr std::size_t CACHE_LINE = 64;
    static constexpr std::size_t MASK = N - 1;

    // head is only written by producer, tail is only written by consumer.
    // Separate cache lines prevent false sharing between the two threads.
    alignas(CACHE_LINE) std::atomic<uint64_t> head_{0};
    char pad_[CACHE_LINE - sizeof(std::atomic<uint64_t>)];
    alignas(CACHE_LINE) std::atomic<uint64_t> tail_{0};

    alignas(CACHE_LINE) std::array<T, N> buffer_;

public:
    // Returns true if the value was pushed, false if the buffer is full.
    bool push(const T& value) {
        const uint64_t h = head_.load(std::memory_order_relaxed);
        if (h - tail_.load(std::memory_order_acquire) == N) {
            return false; // buffer is full
        }
        buffer_[h & MASK] = value;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // Returns the value if one was available, empty optional if the buffer is empty.
    std::optional<T> pop() {
        const uint64_t t = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == t) {
            return std::nullopt; // buffer is empty
        }
        T value = buffer_[t & MASK];
        tail_.store(t + 1, std::memory_order_release);
        return value;
    }

    // Approximate size — not exact under concurrent use, diagnostics only.
    std::size_t size() const {
        const uint64_t h = head_.load(std::memory_order_acquire);
        const uint64_t t = tail_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(h - t);
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() {
        return N;
    }
};