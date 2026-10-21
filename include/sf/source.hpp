#pragma once
#include "sf/imu_sample.hpp"

namespace sf {

enum class ReadResult {
    Ok, // out was filled with a valid sample
    Empty, // no sample was available
    End, // the stream has ended
    Error // an error occurred
}; 

class Source {
public: 
    virtual ~Source() = default; 

    // Fill every field of out, including host_rx_ns
    virtual ReadResult next(ImuSample& out) = 0; 
};

} // namespace sf