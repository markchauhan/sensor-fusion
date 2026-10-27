# Real-Time Sensor Fusion Pipeline

A C++17 pipeline that moves IMU samples from a source thread to a processing thread through a lock-free ring buffer, computes per-axis statistics, and measures its own latency with a fixed-memory histogram. Built to run on simulated data, recorded data, and (next) a real IMU over serial.

## Results

MacBook Pro (Apple Silicon), simulated IMU, `RelWithDebInfo`:

| Mode | Throughput | p50 | p99 | p99.9 | max |
|---|---|---|---|---|---|
| Paced, 1 kHz for 5 s | 1,000 samples/s | 0.43 µs | 24.6 µs | 721 µs | 2.2 ms |
| Unpaced, 2M samples | 7.77 M samples/s | 0.09 µs | 49 µs | 279 µs | 461 µs |

- **Paced** mode is the latency number: samples arrive like a real sensor, so this is how fast the pipeline reacts to one sample.
- **Unpaced** mode is the throughput number: samples are pushed as fast as possible, so its latency includes time waiting in a full queue.
- **p50** is the pipeline itself: one cache-line handoff between cores plus a few multiply-adds. **p99.9 and max** are the OS scheduler briefly taking the core away. macOS cannot pin threads to cores, so this tail reflects the platform, not the code.
- Percentiles are histogram bin upper edges, at most 6.25% above the true value.

## Architecture

```
SimSource ──────┐
ReplaySource ───┼──▶ Source ──[producer thread]──▶ SpscRing ──[consumer thread]──▶ FeatureEngine
RecordingSource ┘    interface                                                   └─▶ LatencyHistogram
(wraps any source)
```

| File | What it does |
|---|---|
| `include/sf/imu_sample.hpp` | One sample: host receive time, device ticks, sequence number, accel, gyro |
| `include/sf/spsc_ring.hpp` | Lock-free single-producer/single-consumer ring buffer |
| `include/sf/latency_histogram.hpp` | Log-linear latency histogram, fixed 7.8 KB, O(1) record |
| `include/sf/source.hpp` | The interface every sample source implements |
| `include/sf/sim_source.hpp` | Simulated still, level IMU: gravity on z plus Gaussian noise |
| `include/sf/recording.hpp` | Record any source to a `.sfr` file and replay it exactly |
| `include/sf/stats.hpp` | Online EWMA and Welford mean/variance |
| `include/sf/feature_engine.hpp` | Per-axis stats for accel and gyro |
| `include/sf/pipeline.hpp` | Owns both threads; lossless shutdown |
| `apps/bench.cpp` | Command-line benchmark |

## Design decisions

**Ring buffer**
- Power-of-2 capacity, so wraparound is a bitmask instead of a divide.
- Indices only grow (`uint64_t`): empty when `head == tail`, full when `head - tail == N`, no extra flag.
- Acquire/release ordering: the release store of an index publishes the slot write that came before it.
- **Cached indices:** each side keeps a private copy of the other side's index and only re-reads the shared atomic when its copy says full or empty. In steady state the producer never touches the consumer's cache line.
- 128-byte alignment between producer and consumer state. Apple Silicon uses 128-byte cache lines, and x86 prefetches lines in pairs.
- Verified with a 10M-item two-thread ordering test under ThreadSanitizer.

**Two clocks per sample**
- `host_rx_ns` is when the sample reached the host, used for latency.
- `mcu_ticks` is when the device sampled it, used for time between samples. Over USB, samples arrive in bursts, so host arrival times give the wrong spacing for integration.
- `seq` detects dropped frames. Tick math is unsigned, so it survives the 32-bit wrap.

**Latency histogram instead of a vector**
- Recording every latency into a growing vector allocates in the hot path, and the reallocation spikes pollute the very tail being measured.
- The histogram uses exact bins for 0-31 ns, then splits every power of two into 16 sub-bins: at most 6.25% error at every scale, 976 bins total, no heap.
- Histograms merge by adding bins, which averages and percentiles cannot do.

**Lossless shutdown**
- The producer sets `producer_done` with release after its last push. The consumer reads it with acquire, then pops once more before exiting. That closes the window where a final sample could land between "ring looked empty" and "producer is done".
- A 200,000-sample unpaced test checks that every sample arrives.

**Statistics**
- EWMA is seeded with the first sample instead of 0, so there is no startup ramp.
- Welford's algorithm in `double`: z-accel has a mean near 9.8 and a variance near 0.01, exactly the case where the naive sum-of-squares formula loses precision.
- Sample variance (divide by n-1), the unbiased estimate of sensor noise.

**Record and replay**
- `RecordingSource` wraps any source, so recording needs no changes to the pipeline.
- `.sfr` files: an 8-byte header (magic, version, record size) followed by raw samples. Bad headers are rejected; a truncated final record is dropped.
- Replay is bit-exact, and the stats from a replayed run match the live run exactly. `--paced` replays with the original spacing from the device ticks.

## Build and run

Requires CMake 3.16+ and a C++17 compiler (tested with Apple clang 16 and GCC).

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure

./build/bench                                  # 1 kHz for 5 s
./build/bench --unpaced --count 2000000        # throughput
./build/bench --record run.sfr                 # benchmark and save the samples
./build/bench --replay run.sfr                 # replay as fast as possible
./build/bench --replay run.sfr --paced         # replay with original timing
```

ThreadSanitizer build:

```bash
cmake -B build-tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build-tsan && ./build-tsan/test_pipeline
```

## Roadmap

- **Wire protocol:** framed binary format with CRC16 and a streaming parser that resynchronizes after corruption.
- **Serial path:** `termios` serial source tested against a fake device on a pseudo-terminal, clock offset and drift estimation, split latency (device to host vs host pipeline).
- **Real data:** EuRoC MAV dataset loader and accuracy against ground truth.
- **Filters:** complementary filter, Madgwick, EKF, each with accuracy and per-sample latency.
- **Fixed-point models:** bit-accurate versions of each filter as golden models for RTL.
- **Hardware:** ISM330DHCX IMU on an STM32, then SPI, timestamping, and filtering on a Zynq FPGA, verified against the fixed-point models.
