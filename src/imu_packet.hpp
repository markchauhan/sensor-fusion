// imu_packet.hpp
// POD struct representing a single IMU sensor reading.

#pragma once

#include <cstdint>

struct ImuPacket {
    uint64_t timestamp_ns;   // nanoseconds since epoch
    float accel_x, accel_y, accel_z;  // m/s^2
    float gyro_x,  gyro_y,  gyro_z;   // rad/s
};