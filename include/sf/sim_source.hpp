#pragma once
#include <chrono> 
#include <cstdint> 
#include <random> 

#include "sf/source.hpp"

namespace sf {

struct SimConfig {
    uint32_t rate_hz = 200; // rate of samples in Hz
    uint64_t count = 0; // number of samples to generate; 0 = infinite
    uint32_t seed = 42; //same seed = same random sequence
    float accel_noise = 0.098f; // stddev of accelerometer noise in m/s^2
    float gyro_noise = 0.01f; // stddev of gyroscope noise in rad/s
};

// Simulated IMU sitting still and level 
class SimSource : public Source {
public: 
    explicit SimSource(const SimConfig& cfg); 
    ReadResult next(ImuSample& out) override; 

private: 
    using Clock = std::chrono::steady_clock; 
    
    SimConfig cfg_; 
    std::mt19937 rng_; 
    std::normal_distribution<float> accel_dist_; 
    std::normal_distribution<float> gyro_dist_;
    Clock::time_point start_; 
    Clock::time_point next_tick_; 
    Clock::duration period_;
    uint32_t seq_ = 0;
    uint64_t produced_ = 0;
};

} // namespace sf