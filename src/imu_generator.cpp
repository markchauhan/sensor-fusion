// imu_generator.cpp
// Simulates an IMU sensor producing packets at a fixed rate
// Runs on a dedicated producer thread, pushing packets into ring-buffer

#include "imu_packet.hpp"
#include "ring_buffer.hpp"
#include <cstdint>
#include <random>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdio>

class ImuGenerator {
public:
    // buffer is owned by the benchmark, generator just pushes packets into it
    ImuGenerator(RingBuffer<ImuPacket, 1024>& buffer, uint32_t rate_hz)
        : buffer_(buffer), rate_hz_(rate_hz), running_(false) {}

    // spawns producer thread, call once
    void start() {
        running_.store(true, std::memory_order_relaxed);
        thread_ = std::thread(&ImuGenerator::run, this);
    }

    void stop() {
        running_.store(false, std::memory_order_relaxed);
        if (thread_.joinable()) thread_.join();
    }

    ~ImuGenerator() { stop();}

private:
    RingBuffer<ImuPacket, 1024>& buffer_;
    uint32_t rate_hz_;
    std::atomic<bool> running_;
    std::thread thread_;

    void run() {
        std::mt19937 rng(42);
        //gaussian noise, realistic IMU sensor noise levels
        std::normal_distribution<float> accel_dist(0.0f, 9.81f * 0.01f);
        std::normal_distribution<float> gyro_dist(0.0f, 0.01f);

        const auto period = std::chrono::nanoseconds(1'000'000'000 / rate_hz_);
        auto next_tick = std::chrono::steady_clock::now();

        while (running_.load(std::memory_order_relaxed)) {
            auto now = std::chrono::steady_clock::now();
            uint64_t timestamp_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    now.time_since_epoch()).count());

                ImuPacket packet{
                    timestamp_ns,
                    accel_dist(rng), accel_dist(rng), accel_dist(rng),
                    gyro_dist(rng), gyro_dist(rng), gyro_dist(rng)
                };

                // backpressure wait if full rather than dropping packets
                while (!buffer_.push(packet) &&
                       running_.load(std::memory_order_relaxed)) {}

                next_tick += period;
                // sleep to absolute time to avoid rate drift 
                std::this_thread::sleep_until(next_tick);    // check if we should stop

        }
    }
};
