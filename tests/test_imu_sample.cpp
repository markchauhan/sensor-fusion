#include "sf/imu_sample.hpp"

int main() {
    sf::ImuSample s{}; 
    s.seq = 1; 
    return s.seq == 1 ? 0 : 1;
}