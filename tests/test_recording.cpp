#include "sf/recording.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "sf/clock.hpp"
#include "sf/pipeline.hpp"
#include "sf/sim_source.hpp"

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            std::fprintf(stderr, "%s:%d CHECK failed: %s\n",          \
                         __FILE__, __LINE__, #cond);                  \
            std::exit(1);                                             \
        }                                                             \
    } while (0)

using namespace sf;
namespace fs = std::filesystem;

static std::string temp_path(const char* name) {
    return (fs::temp_directory_path() / name).string();
}

// Record N sim samples, keep a copy in memory to compare against.
static std::vector<ImuSample> record(const std::string& path, uint64_t n, uint32_t rate) {
    SimConfig cfg;
    cfg.rate_hz = rate;
    cfg.count = n;
    SimSource sim(cfg);
    std::vector<ImuSample> seen;
    {
        RecordingSource rec(sim, path);
        ImuSample s{};
        while (rec.next(s) == ReadResult::Ok) seen.push_back(s);
        CHECK(rec.written() == n);
    }  // destructor closes the file
    return seen;
}

static void test_round_trip_exact() {
    const std::string path = temp_path("sf_test_roundtrip.sfr");
    const auto original = record(path, 1000, 0);

    ReplaySource rep(path, false);
    ImuSample s{};
    for (const ImuSample& o : original) {
        CHECK(rep.next(s) == ReadResult::Ok);
        CHECK(s.seq == o.seq);
        CHECK(s.mcu_ticks == o.mcu_ticks);
        CHECK(std::memcmp(s.accel, o.accel, sizeof s.accel) == 0);  // bit-exact
        CHECK(std::memcmp(s.gyro, o.gyro, sizeof s.gyro) == 0);
    }
    CHECK(rep.next(s) == ReadResult::End);
    CHECK(rep.replayed() == 1000);
    fs::remove(path);
}

static void test_replay_through_pipeline_matches() {
    const std::string path = temp_path("sf_test_pipeline.sfr");

    // Run 1: live sim, recorded.
    SimConfig cfg;
    cfg.rate_hz = 0;
    cfg.count = 50'000;
    SimSource sim(cfg);
    double live_mean = 0.0, live_var = 0.0;
    {
        RecordingSource rec(sim, path);
        Pipeline p(rec, 0.1);
        p.start();
        p.join();
        live_mean = p.engine().accel_stats(2).mean();
        live_var = p.engine().accel_stats(0).variance();
    }

    // Run 2: replay. Same samples in the same order -> identical stats.
    ReplaySource rep(path, false);
    Pipeline p(rep, 0.1);
    p.start();
    p.join();
    CHECK(p.processed() == 50'000);
    CHECK(p.engine().accel_stats(2).mean() == live_mean);   // exactly equal
    CHECK(p.engine().accel_stats(0).variance() == live_var);
    fs::remove(path);
}

static void test_paced_replay_keeps_timing() {
    const std::string path = temp_path("sf_test_paced.sfr");
    record(path, 200, 1000);  // 200 samples, 1 ms apart in mcu_ticks

    ReplaySource rep(path, true);
    ImuSample s{};
    const uint64_t t0 = now_ns();
    while (rep.next(s) == ReadResult::Ok) {}
    const uint64_t ms = (now_ns() - t0) / 1'000'000;
    CHECK(ms >= 190);  // ~199 ms of recorded spacing
    fs::remove(path);
}

static void test_rejects_bad_file() {
    const std::string path = temp_path("sf_test_bad.sfr");
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fputs("definitely not a recording", f);
    std::fclose(f);

    bool threw = false;
    try {
        ReplaySource rep(path, false);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);

    threw = false;
    try {
        ReplaySource rep(temp_path("sf_does_not_exist.sfr"), false);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
    fs::remove(path);
}

static void test_truncated_tail_is_end() {
    const std::string path = temp_path("sf_test_trunc.sfr");
    record(path, 10, 0);
    fs::resize_file(path, fs::file_size(path) - 7);  // cut the last record short

    ReplaySource rep(path, false);
    ImuSample s{};
    uint64_t n = 0;
    while (rep.next(s) == ReadResult::Ok) ++n;
    CHECK(n == 9);  // 9 whole records, the broken one is dropped
    fs::remove(path);
}

int main() {
    test_round_trip_exact();
    test_replay_through_pipeline_matches();
    test_paced_replay_keeps_timing();
    test_rejects_bad_file();
    test_truncated_tail_is_end();
    std::puts("recording: all checks passed");
    return 0;
}