#pragma once
#include <cstdint> 
#include <type_traits>

namespace sf {

struct ImuSample {
    uint64_t host_rx_ns; // host steady_clock when the sample entered the host
    uint32_t mcu_ticks;  // sensor-side time in MCU microseconds; wraps
    uint32_t seq;        // sequence number of the sample
    float accel[3];      // accelerometer sample in m/s^2
    float gyro[3];       // gyroscope sample in rad/s
};

static_assert(std::is_trivially_copyable_v<ImuSample>); 
static_assert(sizeof(ImuSample) == 40, "unexpected size of ImuSample, check for padding/alignment issues");

} // namespace sf