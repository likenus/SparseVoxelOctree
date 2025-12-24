//
// Created by Linus on 22.12.2025.
//

#ifndef SPARSEVOXELOCTREE_TIMER_HPP
#define SPARSEVOXELOCTREE_TIMER_HPP
#include <chrono>

using namespace std::chrono;

class Timer {
    time_point<steady_clock> timer_begin;

public:
    static Timer start() {
        Timer ret = {};
        ret.timer_begin = high_resolution_clock::now();
        return ret;
    }

    [[nodiscard]]
    inline long long lap() const {
        auto time_stamp = high_resolution_clock::now();
        return duration_cast<milliseconds>(time_stamp - timer_begin).count();
    }
};


#endif //SPARSEVOXELOCTREE_TIMER_HPP