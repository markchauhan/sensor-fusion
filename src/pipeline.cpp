#include "sf/pipeline.hpp"

#include "sf/clock.hpp"

namespace sf {

Pipeline::Pipeline(Source& source, double ewma_alpha) 
    : source_(source), engine_(ewma_alpha) {}

Pipeline::~Pipeline() {
    request_stop(); 
    join(); // a joinable std:;thread in a destructor would call std::terminate
}

void Pipeline::start() {
    consumer_ = std::thread(&Pipeline::consumer_loop, this); //consumer first: ready to drain
    producer_ = std::thread(&Pipeline::producer_loop, this);
}

void Pipeline::request_stop() { stop_.store(true, std::memory_order_relaxed); }

void Pipeline::join() {
    if (producer_.joinable()) producer_.join(); 
    if (consumer_.joinable()) consumer_.join(); 
}

void Pipeline::producer_loop() {
    ImuSample s{}; 
    while (!stop_.load(std::memory_order_relaxed)) {
        const ReadResult r = source_.next(s);
        if (r == ReadResult::End) break; 
        if (r == ReadResult::Empty) continue;

        //Ring full: wait for the consumer (backpressure), unless told to stop
        bool pushed = false; 
        while(!(pushed = ring_.try_push(s))) {
            if (stop_.load(std::memory_order_relaxed)) break; 
        }
        if (!pushed) break; 
    }
    // release: every push above is visibile to whoever sees done == true
    producer_done_.store(true, std::memory_order_release);
}

void Pipeline::consumer_loop() {
    ImuSample s{};
    for (;;) {
        if (ring_.try_pop(s)) {
            handle(s);
            continue;
        }
        //empty. If the producer has finished, one last look and done
        if (producer_done_.load(std::memory_order_acquire)) {
            if (!ring_.try_pop(s)) break; 
            handle(s);
        }
        // else: busy-poll for the lowest latency
    }
}

void Pipeline::handle(const ImuSample& s) {
    engine_.update(s);
    latency_.record(now_ns() - s.host_rx_ns);
    ++processed_;
}


} // namespace sf