// bench: run the pipeline and report latency.
//   bench                            1000 Hz sim for 5 s
//   bench --unpaced --count 2000000  throughput
//   bench --record run.sfr           also save the samples
//   bench --replay run.sfr [--paced] play a recording back

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "sf/clock.hpp"
#include "sf/pipeline.hpp"
#include "sf/recording.hpp"
#include "sf/sim_source.hpp"

namespace {

struct Options {
    uint32_t rate_hz = 1000;
    double seconds = 5.0;
    bool unpaced = false;
    uint64_t count = 1'000'000;  // only used with --unpaced
    double alpha = 0.1;
    uint32_t seed = 42;
    std::string record_path;     // empty = don't record
    std::string replay_path;     // empty = use the simulator
    bool paced_replay = false;
};

void usage(const char* prog) {
    std::fprintf(stderr,
                 "usage: %s [--rate HZ] [--seconds S] [--unpaced] [--count N]\n"
                 "          [--alpha A] [--seed N] [--record FILE]\n"
                 "          [--replay FILE [--paced]]\n"
                 "  --rate     sample rate in Hz (default 1000)\n"
                 "  --seconds  run length when paced (default 5)\n"
                 "  --unpaced  no sleeping: push samples as fast as possible\n"
                 "  --count    samples to push with --unpaced (default 1000000)\n"
                 "  --alpha    EWMA smoothing factor (default 0.1)\n"
                 "  --seed     RNG seed for the simulated noise (default 42)\n"
                 "  --record   also write every sample to FILE (.sfr)\n"
                 "  --replay   read samples from FILE instead of the simulator\n"
                 "  --paced    with --replay: keep the recording's original timing\n",
                 prog);
}

// Whole string must be a number, otherwise exit 2.
double to_double(const char* flag, const char* s) {
    char* end = nullptr;
    const double v = std::strtod(s, &end);
    if (end == s || *end != '\0') {
        std::fprintf(stderr, "bad value for %s: '%s'\n", flag, s);
        std::exit(2);
    }
    return v;
}

uint64_t to_u64(const char* flag, const char* s) {
    char* end = nullptr;
    const unsigned long long v = std::strtoull(s, &end, 10);
    if (end == s || *end != '\0' || s[0] == '-') {  // strtoull accepts "-5" and wraps
        std::fprintf(stderr, "bad value for %s: '%s'\n", flag, s);
        std::exit(2);
    }
    return v;
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };

        if (a == "--rate")         o.rate_hz = static_cast<uint32_t>(to_u64("--rate", value()));
        else if (a == "--seconds") o.seconds = to_double("--seconds", value());
        else if (a == "--unpaced") o.unpaced = true;
        else if (a == "--count")   o.count = to_u64("--count", value());
        else if (a == "--alpha")   o.alpha = to_double("--alpha", value());
        else if (a == "--seed")    o.seed = static_cast<uint32_t>(to_u64("--seed", value()));
        else if (a == "--record")  o.record_path = value();
        else if (a == "--replay")  o.replay_path = value();
        else if (a == "--paced")   o.paced_replay = true;
        else if (a == "-h" || a == "--help") { usage(argv[0]); std::exit(0); }
        else {
            std::fprintf(stderr, "unknown flag: %s\n", a.c_str());
            usage(argv[0]);
            std::exit(2);
        }
    }
    if (o.replay_path.empty() && !o.unpaced && (o.rate_hz == 0 || o.seconds <= 0.0)) {
        std::fprintf(stderr, "--rate and --seconds must be > 0\n");
        std::exit(2);
    }
    if (o.paced_replay && o.replay_path.empty()) {
        std::fprintf(stderr, "--paced only makes sense with --replay\n");
        std::exit(2);
    }
    if (!o.record_path.empty() && o.record_path == o.replay_path) {
        std::fprintf(stderr, "--record and --replay can't be the same file\n");
        std::exit(2);
    }
    return o;
}

double us(uint64_t ns) { return static_cast<double>(ns) / 1000.0; }

int run(const Options& opt) {
    std::unique_ptr<sf::Source> base;
    uint64_t expected = 0;  // 0 = unknown up front (replay)

    if (!opt.replay_path.empty()) {
        base = std::make_unique<sf::ReplaySource>(opt.replay_path, opt.paced_replay);
        std::printf("mode: replay %s (%s)\n", opt.replay_path.c_str(),
                    opt.paced_replay ? "original timing" : "as fast as possible");
    } else {
        sf::SimConfig cfg;
        cfg.seed = opt.seed;
        if (opt.unpaced) {
            cfg.rate_hz = 0;
            cfg.count = opt.count;
            std::printf("mode: unpaced (throughput), %llu samples\n",
                        static_cast<unsigned long long>(cfg.count));
        } else {
            cfg.rate_hz = opt.rate_hz;
            cfg.count = static_cast<uint64_t>(opt.rate_hz * opt.seconds);
            std::printf("mode: paced (latency), %u Hz for %.2f s = %llu samples\n",
                        cfg.rate_hz, opt.seconds, static_cast<unsigned long long>(cfg.count));
        }
        expected = cfg.count;
        base = std::make_unique<sf::SimSource>(cfg);
    }

    // Declared after base so it is destroyed first: it holds a reference to base.
    std::unique_ptr<sf::RecordingSource> recorder;
    sf::Source* source = base.get();
    if (!opt.record_path.empty()) {
        recorder = std::make_unique<sf::RecordingSource>(*base, opt.record_path);
        source = recorder.get();
        std::printf("recording to %s\n", opt.record_path.c_str());
    }

    sf::Pipeline pipeline(*source, opt.alpha);
    const uint64_t t0 = sf::now_ns();
    pipeline.start();
    pipeline.join();
    const uint64_t elapsed_ns = sf::now_ns() - t0;

    const auto& h = pipeline.latency();
    const double secs = static_cast<double>(elapsed_ns) / 1e9;
    const double rate = static_cast<double>(pipeline.processed()) / secs;

    std::printf("\nprocessed   %llu samples in %.3f s\n",
                static_cast<unsigned long long>(pipeline.processed()), secs);
    std::printf("throughput  %.0f samples/s\n", rate);

    std::printf("\nhost latency (us)\n");
    std::printf("  min    %10.3f\n", us(h.min()));
    std::printf("  p50    %10.3f\n", us(h.percentile(0.50)));
    std::printf("  p90    %10.3f\n", us(h.percentile(0.90)));
    std::printf("  p99    %10.3f\n", us(h.percentile(0.99)));
    std::printf("  p99.9  %10.3f\n", us(h.percentile(0.999)));
    std::printf("  max    %10.3f\n", us(h.max()));
    std::printf("  mean   %10.3f\n", h.mean() / 1000.0);

    // 6 decimals so a replay can be checked against the original exactly.
    const auto& eng = pipeline.engine();
    std::printf("\nsanity: accel z mean %.6f m/s^2, accel x stddev %.6f m/s^2\n",
                eng.accel_stats(2).mean(), std::sqrt(eng.accel_stats(0).variance()));

    if (pipeline.processed() == 0) return 1;
    if (expected != 0 && pipeline.processed() != expected) return 1;
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse(argc, argv);
    try {
        return run(opt);
    } catch (const std::exception& e) {  // e.g. missing or corrupt .sfr file
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
}
