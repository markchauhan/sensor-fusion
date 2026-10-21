#pragma once
#include <array> 
#include <cstddef> 
#include <cstdint> 

namespace sf {

// Log-linear histogram for latency measurements. Not thread-safe: use one histogram per thread and merge() them to combine. The histogram uses a fixed-size array to store counts of latency measurements in logarithmically spaced bins.
class LatencyHistogram {
public: 
    static constexpr int kSubBits = 4; // 16 sub-bins
    static constexpr uint64_t kSub = uint64_t(1) << kSubBits; // 
    static constexpr std::size_t kBins = (64 - kSubBits + 1) * kSub; // 64-bit values, log2 scale

    void record(uint64_t ns) {
        ++bins_[index_of(ns)]; 
        ++count_;
        sum_ += ns; 
        if (ns < min_) min_ = ns; 
        if (ns > max_) max_ = ns; 
    }

    uint64_t count() const { return count_; }
    uint64_t min() const { return count_ ? min_ : 0; }
    uint64_t max() const { return max_; }
    double mean() const { return count_ ? static_cast<double>(sum_) / count_ : 0.0; }

    // Returns the p-quantile (0.0 <= p <= 1.0) of the recorded latencies. If no latencies have been recorded, returns 0.
    uint64_t percentile(double p) const {
        if (count_ == 0) return 0; 
        uint64_t target = static_cast<uint64_t>(p * static_cast<double>(count_)); 
        if (target < 1) target = 1; 
        if (target > count_) target = count_; 

        uint64_t seen = 0; 
        for (std::size_t i = 0; i < kBins; ++i) {
            seen += bins_[i]; 
            if (seen >= target) {
                const uint64_t ub = upper_bound_of(i);
                return ub < max_ ? ub : max_;
            }
        }
        return max_;
    }

    // Merges another LatencyHistogram into this one. The counts of each bin are added together, and the overall count, sum, min, and max are updated accordingly.
    void merge(const LatencyHistogram& other) {
        for (std::size_t i = 0; i < kBins; ++i) {
            bins_[i] += other.bins_[i]; 
        }
        count_ += other.count_; 
        sum_ += other.sum_; 
        if (other.min_ < min_) min_ = other.min_; 
        if (other.max_ > max_) max_ = other.max_; 
    }

    void reset() { *this = LatencyHistogram{}; }

    // Maps a value to its bin index. Values below 32 get exact bins; larger values are bucketed by their highest set bit plus the next 4 bits.
    static std::size_t index_of(uint64_t v) {
        if (v < 2 * kSub) return static_cast<std::size_t>(v);
        const int msb = 63 - __builtin_clzll(v); // position of the most significant bit
        const int shift = msb - kSubBits;
        const uint64_t sub = (v >> shift) & (kSub - 1); // sub-bin index
        return static_cast<std::size_t>((msb - kSubBits + 1) * kSub + sub); 
    }

    static uint64_t lower_bound_of(std::size_t i) {
        if (i < 2 * kSub) return i;
        const int msb = static_cast<int>(i / kSub) + kSubBits - 1;
        const uint64_t sub = i % kSub;
        return (kSub + sub) << (msb - kSubBits);
    }

    static uint64_t upper_bound_of(std::size_t i) {
        if (i < 2 * kSub) return i; 
        const int msb = static_cast<int>(i / kSub) + kSubBits - 1;
        return lower_bound_of(i) + (uint64_t{1} << (msb - kSubBits)) - 1;
    }

private: 
    std::array<uint64_t, kBins> bins_{}; 
    uint64_t count_ = 0; 
    uint64_t sum_ = 0; 
    uint64_t min_ = UINT64_MAX;
    uint64_t max_ = 0;

};
}   // namespace sf