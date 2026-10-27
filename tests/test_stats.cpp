#include "sf/stats.hpp"
#include "sf/feature_engine.hpp"
#include "sf/pipeline.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

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

static void test_welford_known_values() {
    Welford w;
    for (double x : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0}) w.update(x);
    CHECK(w.count() == 8);
    CHECK(near(w.mean(), 5.0, 1e-12));
    CHECK(near(w.variance(), 32.0 / 7.0, 1e-12));  // sample variance, n-1
}

static void test_welford_large_mean_small_spread() {
    // z-accel: mean ~9.8, tiny spread. This is where naive variance breaks.
    Welford w;
    for (int i = 0; i < 1'000'000; ++i) w.update(i % 2 ? 9.81 : 9.79);
    CHECK(near(w.mean(), 9.80, 1e-9));
    CHECK(near(w.variance(), 0.0001, 1e-9));       // spread of ±0.01
}

static void test_ewma_starts_at_first_sample() {
    Ewma e(0.1);
    e.update(9.81);
    CHECK(near(e.value(), 9.81, 1e-12));   // not 0.981
    for (int i = 0; i < 200; ++i) e.update(0.0);
    CHECK(e.value() < 1e-6);               // decays toward new input
}

static void test_feature_engine_per_axis() {
    FeatureEngine eng(0.1);
    ImuSample s{};
    for (int k = 0; k < 1000; ++k) {
        s.accel[0] = 1.0f;
        s.accel[1] = 2.0f;
        s.accel[2] = (k % 2) ? 9.9f : 9.7f;   // mean 9.8, alternating
        s.gyro[0] = s.gyro[1] = s.gyro[2] = 0.5f;
        eng.update(s);
    }
    CHECK(eng.accel_stats(0).count() == 1000);
    CHECK(near(eng.accel_stats(0).mean(), 1.0, 1e-6));     // axes kept separate
    CHECK(near(eng.accel_stats(1).mean(), 2.0, 1e-6));
    CHECK(near(eng.accel_stats(2).mean(), 9.8, 1e-5));
    CHECK(eng.accel_stats(0).variance() < 1e-12);          // constant axis: no spread
    CHECK(eng.accel_stats(2).variance() > 0.009);          // alternating axis: spread
    CHECK(near(eng.gyro_ewma(1).value(), 0.5, 1e-6));      // gyro side wired up too
}

int main() {
    test_welford_known_values();
    test_welford_large_mean_small_spread();
    test_ewma_starts_at_first_sample();
    test_feature_engine_per_axis();
    std::puts("stats: all checks passed");
    return 0;
}