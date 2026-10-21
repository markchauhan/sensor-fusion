#include "sf/sim_source.hpp"

#include <thread>
#include "sf/clock.hpp"

namespace sf {

namespace {
constexpr float kGravity = 9.80665f; // m/s^2
}

SimSource::SimSource(const SimConfig& cfg) 
    : cfg_(cfg), 
      rng_(cfg.seed), 
      accel_dist_(0.0f, cfg.accel_noise), 
      gyro_dist_(0.0f, cfg.gyro_noise),
      start_(Clock::now()), 
      next_tick_(start_), 
      period_(cfg.rate_hz ? std::chrono::nanoseconds(1'000'000'000 / cfg.rate_hz)
                          : Clock::duration::zero()) {}

ReadResult SimSource::next(ImuSample& out) {
    if (cfg_.count != 0 && produced_ >= cfg_.count) return ReadResult::End; 

    if (cfg_.rate_hz != 0) {
        std::this_thread::sleep_until(next_tick_);
        next_tick_ += period_; 
    }

    const auto elapsed = Clock::now() - start_; 
    out.mcu_ticks = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
    out.seq = seq_++;

    out.accel[0] = accel_dist_(rng_);
    out.accel[1] = accel_dist_(rng_);
    out.accel[2] = kGravity + accel_dist_(rng_);
    out.gyro[0] = gyro_dist_(rng_);
    out.gyro[1] = gyro_dist_(rng_);
    out.gyro[2] = gyro_dist_(rng_);

    out.host_rx_ns = now_ns();  // last: the moment the sample "arrives"
    ++produced_;
    return ReadResult::Ok;
}

} // namespace sf