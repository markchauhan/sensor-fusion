// benchmark.cpp
// Brings the ring buffer, IMU generator, and feature engine
// Runs for a fixed duration then reports throughput (packets/sec) and latency (ms)

#include "imu_packet.hpp"
#include "ring_buffer.hpp"
#include "imu_generator.cpp"
#include "feature_engine.cpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <thread>

int main() {
    constexpr int DURATION_SEC = 5; 
    constexpr uint32_t RATE_HZ  = 200;
    constexpr float    ALPHA    = 0.1f;

    RingBuffer<ImuPacket, 1024> buffer;
    ImuGenerator  generator(buffer, RATE_HZ);
    FeatureEngine engine(buffer, ALPHA);

    printf("Running benchmark: %d seconds at %u Hz...\n", DURATION_SEC, RATE_HZ);
    
    auto t_start = std::chrono::steady_clock::now();

    engine.start();
    generator.start();

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SEC));

    generator.stop();
    engine.stop();

    auto t_end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t_end - t_start).count();

    //throughput
    uint64_t total = engine.packets_processed();
    double throughput = static_cast<double>(total) / elapsed;

    printf("Total packets processed: %lu\n", total);
    printf("Throughput: %.2f packets/sec\n", throughput);
 
    // latency percentiles
    auto samples = engine.latency_samples();
    std::sort(samples.begin(), samples.end());

    if (!samples.empty()) {
        auto p50 = samples[samples.size() / 2];
        auto p90 = samples[static_cast<size_t>(samples.size() * 0.9)];
        auto p99 = samples[static_cast<size_t>(samples.size() * 0.99)];

        printf("Latency (us): p50=%.3f, p90=%.3f, p99=%.3f\n", p50 / 1000.0, p90 / 1000.0, p99 / 1000.0);
    } else {
        printf("No latency samples collected.\n");
    }

    return 0; 
}

