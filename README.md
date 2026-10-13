# Real-Time Sensor Fusion Pipeline

Lock-free SPSC pipeline in C++17 processing a simulated 200 Hz IMU stream, with sub-microsecond median handoff latency on a shared VM.

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

What these numbers mean:

- **Throughput is set by the generator, not the pipeline.** The producer sleeps until each 5 ms tick (`sleep_until`), so the benchmark always processes about 200 packets/sec. It does not measure the buffer's maximum rate.
- **Latency** is the time from the producer's timestamp, taken just before it builds the packet, to the consumer finishing that packet's feature updates. It covers packet creation, the ring-buffer handoff and the EWMA and variance updates.
- **The p99 spike** reflects OS scheduler jitter on a shared VM, not pipeline overhead. Neither thread is pinned to a core.
- **Results vary by machine.** A later run on a 2-core Linux VM gave p50 of about 1 µs with throughput again pinned at 200 packets/sec.
- **Variance** is the population variance (`M2 / n`).

## Build

```bash
cmake -B build && cmake --build build
./build/benchmark
```

## Roadmap

- **Next:** a saturation benchmark (producer pushes with no sleep) to measure the buffer's maximum throughput, with both threads pinned to separate cores

- **Level 2:** GPS stream, thread pool, complementary filter fusion
- **Level 3:** Real EuRoC MAV dataset, Extended Kalman Filter, published write-up
