#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

#include "sf/clock.hpp"
#include "sf/source.hpp"

namespace sf {
 // .sfr file: 8-byte header, then raw ImuSample records back to back.
//   bytes 0-3  magic "SFR1"
//   bytes 4-5  format version (1)
//   bytes 6-7  record size in bytes (40)

inline constexpr char kSfrMagic[4] = {'S', 'F', 'R', '1'};
inline constexpr uint16_t kSfrVersion = 1;

//Wraps any Source and writes every sample 
// Pipeline seees exactly the saem samples as w/o recording
class RecordingSource final : public Source {
public:
    RecordingSource(Source& inner, const std::string& path);
    ~RecordingSource() override;

    RecordingSource(const RecordingSource&) = delete;
    RecordingSource& operator=(const RecordingSource&) = delete;

    ReadResult next(ImuSample& out) override;
    uint64_t written() const { return written_; }

private:
    Source& inner_;
    std::FILE* file_ = nullptr;
    uint64_t written_ = 0;
};

// Plays a .sfr file back through the pipeline.
class ReplaySource final : public Source {
public:
    ReplaySource(const std::string& path, bool paced);
    ~ReplaySource() override;

    ReplaySource(const ReplaySource&) = delete;
    ReplaySource& operator=(const ReplaySource&) = delete;

    ReadResult next(ImuSample& out) override;
    uint64_t replayed() const { return replayed_; }

private:
    std::FILE* file_ = nullptr;
    bool paced_;
    uint64_t replayed_ = 0;

    // pacing state
    uint64_t start_ns_ = 0;     // host time of the first sample
    uint32_t prev_ticks_ = 0;   // last sample's mcu_ticks
    uint64_t elapsed_us_ = 0;   // device time since the first sample
};


} // namespace sf