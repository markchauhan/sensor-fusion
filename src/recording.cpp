#include "sf/recording.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace sf {

namespace {

struct SfrHeader {
    char magic[4];
    uint16_t version;
    uint16_t record_size;
};
static_assert(sizeof(SfrHeader) == 8, "header must be 8 bytes");

}  // namespace

// ---------- RecordingSource ----------

RecordingSource::RecordingSource(Source& inner, const std::string& path)
    : inner_(inner) {
    file_ = std::fopen(path.c_str(), "wb");
    if (!file_) throw std::runtime_error("cannot open for writing: " + path);

    // Big buffer: most writes are a memcpy
    std::setvbuf(file_, nullptr, _IOFBF, 1 << 20);

    SfrHeader h{};
    std::memcpy(h.magic, kSfrMagic, sizeof h.magic);
    h.version = kSfrVersion;
    h.record_size = static_cast<uint16_t>(sizeof(ImuSample));
    if (std::fwrite(&h, sizeof h, 1, file_) != 1)
        throw std::runtime_error("cannot write header: " + path);
}

RecordingSource::~RecordingSource() {
    if (file_) std::fclose(file_);  // flushes the buffer
}

ReadResult RecordingSource::next(ImuSample& out) {
    const ReadResult r = inner_.next(out);
    if (r == ReadResult::Ok) {
        std::fwrite(&out, sizeof out, 1, file_);
        ++written_;
    }
    return r;
}

// ------- ReplaySource ----------
ReplaySource::ReplaySource(const std::string& path, bool paced) : paced_(paced) {
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) throw std::runtime_error("cannot open for reading: " + path);

    SfrHeader h{};
    if (std::fread(&h, sizeof h, 1, file_) != 1 ||
        std::memcmp(h.magic, kSfrMagic, sizeof h.magic) != 0) {
        std::fclose(file_);
        file_ = nullptr;
        throw std::runtime_error("not a .sfr recording: " + path);
    }
    if (h.version != kSfrVersion || h.record_size != sizeof(ImuSample)) {
        std::fclose(file_);
        file_ = nullptr;
        throw std::runtime_error("unsupported .sfr version or record size: " + path);
    }
}

ReplaySource::~ReplaySource() {
    if (file_) std::fclose(file_);
}

ReadResult ReplaySource::next(ImuSample& out) {
    // A partial record at the end (file cut off mid-write) means End
    if (std::fread(&out, sizeof out, 1, file_) != 1) return ReadResult::End;

    if (paced_) {
        if (replayed_ == 0) {
            start_ns_ = now_ns();
        } else {
            elapsed_us_ += out.mcu_ticks - prev_ticks_;  // unsigned: survives wrap
        }
        prev_ticks_ = out.mcu_ticks;

        const uint64_t due_ns = start_ns_ + elapsed_us_ * 1000;
        const uint64_t now = now_ns();
        if (due_ns > now)
            std::this_thread::sleep_for(std::chrono::nanoseconds(due_ns - now));
    }

    out.host_rx_ns = now_ns();  // re-stamp: latency is about THIS run
    ++replayed_;
    return ReadResult::Ok;
}

}  // namespace sf