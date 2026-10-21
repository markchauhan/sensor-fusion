#include "sf/sim_source.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "sf/clock.hpp"

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            std::fprintf(stderr, "%s:%d CHECK failed: %s\n",          \
                         __FILE__, __LINE__, #cond);                  \
            std::exit(1);                                             \
        }                                                             \
    } while (0)

using namespace sf;

static void test_count_and_end() {
    SimConfig cfg;
    cfg.rate_hz = 0;
    cfg.count = 1000;
    std::unique_ptr<Source> src = std::make_unique<SimSource>(cfg);  // via the interface

    ImuSample s{};
    uint64_t n = 0;
    uint32_t last_ticks = 0;
    uint64_t last_rx = 0;
    double z_sum = 0.0;

    while (src->next(s) == ReadResult::Ok) {
        CHECK(s.seq == n);                 // consecutive, no gaps
        CHECK(s.mcu_ticks >= last_ticks);  // device clock never goes back
        CHECK(s.host_rx_ns >= last_rx);    // host clock never goes back
        last_ticks = s.mcu_ticks;
        last_rx = s.host_rx_ns;
        z_sum += s.accel[2];
        ++n;
    }
    CHECK(n == 1000);
    CHECK(src->next(s) == ReadResult::End);  // stays ended

    const double z_mean = z_sum / static_cast<double>(n);
    CHECK(std::fabs(z_mean - 9.80665) < 0.05);  // gravity shows up
}

static void test_same_seed_same_values() {
    SimConfig cfg;
    cfg.rate_hz = 0;
    cfg.count = 50;
    SimSource a(cfg), b(cfg);
    ImuSample sa{}, sb{};
    while (a.next(sa) == ReadResult::Ok) {
        CHECK(b.next(sb) == ReadResult::Ok);
        for (int i = 0; i < 3; ++i) {
            CHECK(sa.accel[i] == sb.accel[i]);
            CHECK(sa.gyro[i] == sb.gyro[i]);
        }
    }
}

static void test_pacing() {
    SimConfig cfg;
    cfg.rate_hz = 1000;  // 1 ms apart
    cfg.count = 200;
    SimSource src(cfg);
    ImuSample s{};
    const uint64_t t0 = now_ns();
    while (src.next(s) == ReadResult::Ok) {}
    const uint64_t elapsed_ms = (now_ns() - t0) / 1'000'000;
    CHECK(elapsed_ms >= 195);  // 200 samples at 1 kHz can't finish early
}

int main() {
    test_count_and_end();
    test_same_seed_same_values();
    test_pacing();
    std::puts("sim_source: all checks passed");
    return 0;
}