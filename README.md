# Real-Time Sensor Fusion Pipeline

Lock-free SPSC pipeline in C++17 processing simulated IMU sensor streams with sub-microsecond median latency.

## Architecture

Three components wired together in `benchmark.cpp`:
- `ring_buffer.hpp` — templated lock-free SPSC buffer, owns no threads
- `imu_generator.cpp` — producer thread, pushes simulated IMU packets at a fixed rate
- `feature_engine.cpp` — consumer thread, computes per-axis EWMA and Welford variance

## Design Decisions

- **Lock-free SPSC ring buffer** with acquire/release memory ordering — sufficient for single producer/consumer, cheaper than seq_cst
- **Power-of-2 capacity** for bitmask wraparound (`idx & (N-1)`) instead of modulo division in the hot path
- **Cache-line-aligned atomics** — head and tail on separate 64-byte cache lines to eliminate false sharing between producer and consumer cores
- **uint64_t indices that grow forever** — empty when head == tail, full when head - tail == N, no explicit flag needed
- **Online EWMA and Welford variance** — O(1) time and O(1) memory per sample, no heap allocation in the hot path

## Benchmark Results

Codespace VM (shared), 5 seconds at 200Hz:

| Metric | Value |
|--------|-------|
| Packets processed | 1,000 |
| Throughput | 199.89 packets/sec |
| Latency p50 | 0.56 µs |
| Latency p90 | 4.92 µs |
| Latency p99 | 2,112 µs |

Note: p99 spike reflects OS scheduler jitter on a shared VM, not pipeline overhead.

## Build

```bash
cmake -B build && cmake --build build
./build/benchmark
```

## Roadmap

- **Level 2:** GPS stream, thread pool, complementary filter fusion
- **Level 3:** Real EuRoC MAV dataset, Extended Kalman Filter, published write-up
