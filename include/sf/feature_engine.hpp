#pragma once
#include <array> 

#include "sf/imu_sample.hpp"
#include "sf/stats.hpp"

namespace sf {

//per-axis ewma and welford stats for accel and gyro
// no clocks, so it can be tested on its own
class FeatureEngine {
public: 
    explicit FeatureEngine(double alpha)
        : accel_ewma_{Ewma(alpha), Ewma(alpha), Ewma(alpha)},
          gyro_ewma_{Ewma(alpha), Ewma(alpha), Ewma(alpha)} {}
        
    void update(const ImuSample& s) {
        for (int i = 0; i < 3; ++i) {
            accel_ewma_[i].update(s.accel[i]);
            accel_stats_[i].update(s.accel[i]);
            gyro_ewma_[i].update(s.gyro[i]);
            gyro_stats_[i].update(s.gyro[i]);
        }
    }

    const Ewma& accel_ewma(int axis) const { return accel_ewma_[axis]; }
    const Ewma& gyro_ewma(int axis) const { return gyro_ewma_[axis]; }
    const Welford& accel_stats(int axis) const { return accel_stats_[axis]; }
    const Welford& gyro_stats(int axis) const { return gyro_stats_[axis]; }

private: 
    std::array<Ewma, 3> accel_ewma_;
    std::array<Ewma, 3> gyro_ewma_;
    std::array<Welford, 3> accel_stats_{};
    std::array<Welford, 3> gyro_stats_{};
};

} // namespace sf