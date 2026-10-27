#pragma once
#include <atomic> 
#include <cstdint> 
#include <thread> 

#include "sf/feature_engine.hpp"
#include "sf/imu_sample.hpp"
#include "sf/latency_histogram.hpp"
#include "sf/source.hpp"
#include "sf/spsc_ring.hpp"

namespace sf {

using ImuRing = SpscRing<ImuSample, 1024>;

// Source -> [producer thread] -> ring -> [consumer thread]-> Feature Engine
// Read engine(), latency(), processed() only after join()
class Pipeline {
public: 
    Pipeline(Source& source, double ewma_alpha); 
    ~Pipeline(); 

    Pipeline(const Pipeline&) = delete; //owns threads: no copies
    Pipeline& operator=(const Pipeline&) = delete; 

    void start(); 
    void request_stop(); // for endless sources; finite ones stop on End
    void join(); 

    const FeatureEngine& engine() const { return engine_; }
    const LatencyHistogram& latency() const { return latency_; }
    uint64_t processed() const {return processed_; }


private: 
    void producer_loop(); 
    void consumer_loop(); 
    void handle(const ImuSample& s); 

    Source& source_;
    ImuRing ring_; 
    FeatureEngine engine_; 
    LatencyHistogram latency_;
    uint64_t processed_ = 0;

    std::atomic<bool> stop_{false};
    std::atomic<bool> producer_done_{false};
    std::thread producer_;
    std::thread consumer_;
};

} // namespace sf