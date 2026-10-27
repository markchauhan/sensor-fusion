#include "sf/stats.hpp"

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

int main() {
    test_welford_known_values();
    test_welford_large_mean_small_spread();
    test_ewma_starts_at_first_sample();
    std::puts("stats: all checks passed");
    return 0;
}