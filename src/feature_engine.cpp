// feature_engine.cpp
// Consumes IMU packets from ring buffer and computes per-axis
// EWMA and rolling variance using Welford's algorithm

#include "imu_packet.hpp"
#include "ring_buffer.hpp"
#include <cstdint>
#include <atomic>
#include <thread>
#include <cstdio>
#include <vector>

struct EWMAState {
    float value = 0.0f; 
    const float alpha; 

    explicit EWMAState(float alpha) : alpha(alpha) {}

    void update(float sample) {
        value = alpha * sample + (1.0f - alpha) * value;
    }
};

struct WelfordState {
    uint64_t count = 0; 
    float mean = 0.0f; 
    float M2 = 0.0f; 

    void update(float sample) {
        count++;
        float delta = sample - mean;
        mean += delta / static_cast<float>(count);
        float delta2 = sample - mean;
        M2 += delta * delta2;
    }

    float variance() const {
        return count < 2 ? 0.0f : M2 / static_cast<float>(count);
    }
};

class FeatureEngine {
    public: 
        FeatureEngine(RingBuffer<ImuPacket, 1024>& buffer, float alpha)
            : buffer_(buffer), running_(false), packets_processed_(0), 
            accel_ewma_x(alpha), accel_ewma_y(alpha), accel_ewma_z(alpha),
            gyro_ewma_x(alpha), gyro_ewma_y(alpha), gyro_ewma_z(alpha) 
        {}

        void start() {
            running_.store(true, std::memory_order_relaxed);
            thread_ = std::thread(&FeatureEngine::run, this);
        }

        void stop() {
            running_.store(false, std::memory_order_relaxed);
            if (thread_.joinable()) thread_.join();
        }
        
        uint64_t packets_processed() const {
            return packets_processed_.load(std::memory_order_relaxed);
        }


        const std::vector<uint64_t>& latency_samples() const{
            return latency_samples_;
        }

        ~FeatureEngine() { stop();}

    private: 
        RingBuffer<ImuPacket, 1024>& buffer_;
        std::atomic<bool> running_;
        std::atomic<uint64_t> packets_processed_;
        std::vector<uint64_t> latency_samples_;
        std::thread thread_;

        // per-axis state
        EWMAState accel_ewma_x, accel_ewma_y, accel_ewma_z;
        EWMAState gyro_ewma_x, gyro_ewma_y, gyro_ewma_z;

        WelfordState accel_var_x, accel_var_y, accel_var_z;
        WelfordState gyro_var_x, gyro_var_y, gyro_var_z;

        void run() {
            while (running_.load(std::memory_order_relaxed)) {
                auto packet = buffer_.pop(); 
                if(!packet) continue; 

                //update accel state
                accel_ewma_x.update(packet->accel_x);
                accel_ewma_y.update(packet->accel_y);
                accel_ewma_z.update(packet->accel_z);

                accel_var_x.update(packet->accel_x);
                accel_var_y.update(packet->accel_y);
                accel_var_z.update(packet->accel_z);

                //update gyro state
                gyro_ewma_x.update(packet->gyro_x);
                gyro_ewma_y.update(packet->gyro_y);
                gyro_ewma_z.update(packet->gyro_z);

                gyro_var_x.update(packet->gyro_x);
                gyro_var_y.update(packet->gyro_y);
                gyro_var_z.update(packet->gyro_z);

                auto now_ns = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                
                latency_samples_.push_back(now_ns - packet->timestamp_ns);
                packets_processed_.fetch_add(1, std::memory_order_relaxed);
            }
        }
};