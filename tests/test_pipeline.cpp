#include "sf/pipeline.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

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

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static void test_finite_run_processes_everything() {
    SimConfig cfg;
    cfg.rate_hz = 0;        // unpaced: stress the ring
    cfg.count = 200'000;
    SimSource src(cfg);

    Pipeline p(src, 0.1);
    p.start();
    p.join();

    CHECK(p.processed() == 200'000);
    CHECK(p.latency().count() == 200'000);
    CHECK(p.engine().accel_stats(2).count() == 200'000);

    // Engine sees gravity on z and the configured noise.
    CHECK(near(p.engine().accel_stats(2).mean(), 9.80665, 0.01));
    const double sd = std::sqrt(p.engine().accel_stats(0).variance());
    CHECK(near(sd, 0.098, 0.005));
}

static void test_stop_endless_source() {
    SimConfig cfg;
    cfg.rate_hz = 1000;
    cfg.count = 0;          // forever
    SimSource src(cfg);

    Pipeline p(src, 0.1);
    p.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    p.request_stop();
    p.join();               // must return, not hang

    CHECK(p.processed() > 50);  // ~100 expected; loose bound for busy machines
}

int main() {
    test_finite_run_processes_everything();
    test_stop_endless_source();
    std::puts("pipeline: all checks passed");
    return 0;
}