#include "sf/latency_histogram.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            std::fprintf(stderr, "%s:%d CHECK failed: %s\n",          \
                         __FILE__, __LINE__, #cond);                  \
            std::exit(1);                                             \
        }                                                             \
    } while (0)

using sf::LatencyHistogram;

static void test_empty() {
    LatencyHistogram h;
    CHECK(h.count() == 0);
    CHECK(h.min() == 0);
    CHECK(h.max() == 0);
    CHECK(h.percentile(0.5) == 0);
}

static void test_small_values_exact() {
    LatencyHistogram h;
    for (uint64_t v = 0; v < 32; ++v) {
        CHECK(LatencyHistogram::index_of(v) == v);
        CHECK(LatencyHistogram::lower_bound_of(v) == v);
        CHECK(LatencyHistogram::upper_bound_of(v) == v);
    }
    for (uint64_t v = 1; v <= 10; ++v) h.record(v);
    CHECK(h.count() == 10);
    CHECK(h.min() == 1);
    CHECK(h.max() == 10);
    CHECK(h.percentile(0.5) == 5);
    CHECK(h.percentile(1.0) == 10);
}

static void test_every_value_lands_in_its_bin() {
    // Walk values across many magnitudes, including bin edges.
    for (int k = 5; k < 63; ++k) {
        const uint64_t base = uint64_t{1} << k;
        const uint64_t probes[] = {base, base + 1, base + base / 3, base * 2 - 1};
        for (uint64_t v : probes) {
            const std::size_t i = LatencyHistogram::index_of(v);
            CHECK(i < LatencyHistogram::kBins);
            CHECK(LatencyHistogram::lower_bound_of(i) <= v);
            CHECK(v <= LatencyHistogram::upper_bound_of(i));
            // bin width is at most 1/16 of the value
            const uint64_t width = LatencyHistogram::upper_bound_of(i)
                                 - LatencyHistogram::lower_bound_of(i) + 1;
            CHECK(width * 16 <= v);
        }
    }
    CHECK(LatencyHistogram::index_of(UINT64_MAX) == LatencyHistogram::kBins - 1);
}

static void test_percentiles_within_error() {
    LatencyHistogram h;
    for (uint64_t v = 1; v <= 100000; ++v) h.record(v);
    const uint64_t p50 = h.percentile(0.50);
    const uint64_t p99 = h.percentile(0.99);
    CHECK(p50 >= 50000 && p50 <= 50000 + 50000 / 16);
    CHECK(p99 >= 99000 && p99 <= 99000 + 99000 / 16);
    CHECK(h.percentile(1.0) == 100000);
}

static void test_merge() {
    LatencyHistogram a, b;
    a.record(100);
    b.record(5);
    b.record(1'000'000);
    a.merge(b);
    CHECK(a.count() == 3);
    CHECK(a.min() == 5);
    CHECK(a.max() == 1'000'000);
}

int main() {
    test_empty();
    test_small_values_exact();
    test_every_value_lands_in_its_bin();
    test_percentiles_within_error();
    test_merge();
    std::printf("latency_histogram: all checks passed (%zu bins, %zu bytes)\n",
                LatencyHistogram::kBins, sizeof(LatencyHistogram));
    return 0;
}