#include "sf/spsc_ring.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

// assert() is compiled out in RelWithDebInfo (NDEBUG is set), so use our own.
#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            std::fprintf(stderr, "%s:%d CHECK failed: %s\n",          \
                         __FILE__, __LINE__, #cond);                  \
            std::exit(1);                                             \
        }                                                             \
    } while (0)

static void test_single_thread() {
    sf::SpscRing<int, 4> ring;
    int v = -1;

    CHECK(!ring.try_pop(v));                       // starts empty

    for (int i = 0; i < 4; ++i) CHECK(ring.try_push(i));
    CHECK(!ring.try_push(99));                     // full at capacity

    for (int i = 0; i < 4; ++i) {
        CHECK(ring.try_pop(v));
        CHECK(v == i);                             // FIFO order
    }
    CHECK(!ring.try_pop(v));                       // empty again

    for (int i = 0; i < 1000; ++i) {               // wraps the index many times
        CHECK(ring.try_push(i));
        CHECK(ring.try_pop(v));
        CHECK(v == i);
    }
}

static void test_two_threads() {
    constexpr uint64_t kCount = 10'000'000;
    static sf::SpscRing<uint64_t, 1024> ring;      // static: too big for comfort on the stack

    std::thread producer([] {
        for (uint64_t i = 0; i < kCount; ++i)
            while (!ring.try_push(i)) {}           // spin while full
    });

    uint64_t expected = 0, v = 0;
    while (expected < kCount) {
        if (ring.try_pop(v)) {
            CHECK(v == expected);                  // no loss, no dupes, in order
            ++expected;
        }
    }
    producer.join();
    CHECK(!ring.try_pop(v));
}

int main() {
    test_single_thread();
    test_two_threads();
    std::puts("spsc_ring: all checks passed");
    return 0;
}