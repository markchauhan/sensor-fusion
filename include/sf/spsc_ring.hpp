#pragma once
#include <atomic>
#include <cstddef> 
#include <cstdint> 
#include <type_traits> 

namespace sf {

// 128, not 64: Apple Silicon uses 128-byte cache lines
// x86 the adjacent-line prefetcher pulls lines in pairs, so 128 is better 
inline constexpr std::size_t kCacheLineSize = 128;

template <typename T, std::size_t N>

class SpscRing {
    static_assert(N >= 2&& (N & (N - 1)) == 0, "N must be a power of two and >= 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable"); 

public: 
    // producer thread
    bool try_push(const T& item) {
        const uint64_t h = head_.load(std::memory_order_relaxed); // read head without synchronizing with consumer
        if (h - tail_cache_ == N) {
            tail_cache_ = tail_.load(std::memory_order_acquire); // refresh
            if (h - tail_cache_ == N) {
                return false; // full
            }
        }

        slots_[h & kMask] = item;  // write to the slot
        head_.store(h + 1, std::memory_order_release); // publish the new head
        return true;
    }

    //consumer thread 
    bool try_pop(T& out) {
        const uint64_t t = tail_.load(std::memory_order_relaxed); 
        if (t == head_cache_) {
            head_cache_ = head_.load(std::memory_order_acquire); // refresh
            if (t == head_cache_) {
                return false; // empty
            }
        }

        out = slots_[t & kMask]; // read from the slot
        tail_.store(t + 1, std::memory_order_release); // publish the new
        return true;
    }

    static constexpr std::size_t capacity() { return N; }

private: 
    static constexpr uint64_t kMask = N - 1;

    //producer cahce line: written by producer, read by consumer
    alignas(kCacheLineSize) std::atomic<uint64_t> head_{0};
    uint64_t tail_cache_{0}; // producer's private copy of tail_

    //consumer cache line: written by consumer, read by producer
    alignas(kCacheLineSize) std::atomic<uint64_t> tail_{0};
    uint64_t head_cache_{0}; // consumer's private copy of head_

    alignas(kCacheLineSize) T slots_[N]{}; // ring buffer slots
};

} // namespace sf