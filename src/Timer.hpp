//
// Created by Linus on 22.12.2025.
//

#ifndef SPARSEVOXELOCTREE_TIMER_HPP
#define SPARSEVOXELOCTREE_TIMER_HPP
#include <chrono>
#include <ranges>

#include "spdlog/spdlog.h"

using namespace std::chrono;

class Timer {
    time_point<steady_clock> timer_begin;
    std::vector<std::pair<std::string, std::common_type_t<duration<long long, std::ratio<1, 1000000000>>, duration<long long, std::ratio<1, 1000000000>>>>> laps;

public:
    static std::shared_ptr<Timer> start() {
        auto ret = std::make_shared<Timer>();
        ret->timer_begin = high_resolution_clock::now();
        return ret;
    }

    inline long long lap(std::string description = "") {
        auto time_stamp = high_resolution_clock::now();
        auto duration = time_stamp - timer_begin;
        laps.emplace_back(description, duration);
        return duration_cast<milliseconds>(duration).count();
    }

    void new_lap() {
        timer_begin = high_resolution_clock::now();
    }

    long long total() const {
        long long sum_of_all_segments = 0;
        for (auto duration : laps | std::views::values) {
            sum_of_all_segments += duration_cast<milliseconds>(duration).count();
        }
        return sum_of_all_segments;
    }

    void log(const std::string &title = "") {
        spdlog::info(title);
        for (auto [description, duration] : laps) {
            spdlog::info("{} | {} ms", description, duration_cast<milliseconds>(duration).count());
        }
        spdlog::info("Total: {} ms", this->total());
    }
};


#endif //SPARSEVOXELOCTREE_TIMER_HPP