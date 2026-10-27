#pragma once 
#include <cstdint> 

namespace sf {

// Exponentially weighted moving average: recent samples count more. 
// value = alpha * sample + (1 - alpha) * value
class Ewma {
    public: 
        explicit Ewma(double alpha) : alpha_(alpha) {}

        void update(double x) {
            if (!primed_) { //first sample: start AT the data, not at 0
                value_ = x; 
                primed_ = true;
                return;
            }
            value_ += alpha_ * (x - value_); 
        }
        double value() const {return value_; }

    private:
        double alpha_; 
        double value_ = 0.0;
        bool primed_ = false;
};

// Welford's online mean and variance: one pass, O(1) memory, numerically stable
class Welford {
public: 
    void update(double x) {
        ++n_;
        const double delta = x - mean_;
        mean_ += delta / static_cast<double>(n_);
        m2_ += delta * (x - mean_); // uses the OLD and NEW mean
    }

    uint64_t count() const { return n_; }
    double mean() const { return mean_; }

    // Sample variance (divide by n - 1) 
    double variance() const { return n_ > 1 ? m2_ / static_cast<double>(n_ - 1) : 0.0; }

private: 
    uint64_t n_ = 0;
    double mean_ = 0.0; 
    double m2_ = 0.0; 
};

} // namespace sf