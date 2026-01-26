//
// Created by linus on 08.11.2025.
//

#include "VoxelGenerator.h"
#include <unordered_map>
#include <chrono>

#include "Quaternion.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "myvk/CommandBuffer.hpp"
#include "spdlog/spdlog.h"

#define COLOR_MASK 0x00ffffffu

constexpr uint32_t COLORS[8] = {0x00ff0000, 0x0000ff00, 0x000000ff, 0x0000ffff, 0x00ff00ff, 0x00ffff00, 0x00ffffff, 0x000f0f0f};

std::shared_ptr<VoxelGenerator> VoxelGenerator::Create(
    const std::shared_ptr<Timer> &timer,
    const std::shared_ptr<LSystem> &ls,
    const uint32_t octree_level,
    const uint32_t depth) {

    auto ret = std::make_shared<VoxelGenerator>();
    timer->new_lap();
    ret->lsystem = ls;
    ret->m_sequence = ls->iterate(std::max(1u, depth));
    timer->lap("iterating string");
    ret->m_voxel_resolution = 1u << octree_level;
    ret->step = 1.f / static_cast<float>(ret->m_voxel_resolution);
    ret->m_octree_level = octree_level;
    ret->m_depth = depth;
    ret->delta = ls->delta * static_cast<float>(M_PI / 180.f);
    ret->sin_delta = glm::sin(ret->delta / 2.f);
    ret->cos_delta = glm::cos(ret->delta / 2.f);
    ret->stack.reserve(depth + 1);
    ret->m_num_terminals = count_terminals(ls);

    return ret;
}


uint32_t VoxelGenerator::estimate_fragment_count() const {
    return std::pow(m_num_terminals, m_depth);
}

bool VoxelGenerator::DoStep(glm::uvec4 &out_pos) {
    if (m_pos >= m_sequence.length()) {
        is_done = true;
        return false; // Potentially grow
    }

    while (m_pos < m_sequence.length()) {

        switch (char c = m_sequence[m_pos++]) {
            case 'F': {
                t.p += t.d * step;
                auto pos = glm::round(t.p * static_cast<float>(m_voxel_resolution));

                uint32_t color = COLORS[stack.size() % 8] & 0x00ffffff;
                int voxel_size = clamp(static_cast<int>(m_octree_level / 2 - stack.size()), 0, 6) << 24; // voxel_size = log2(actual size)
                voxel_size = 0 << 24;
                out_pos = glm::uvec4(pos, color | voxel_size);

                return true;
            }
            case '[':
                stack.push_back(t); break;
            case ']':
                t = stack.back();
                stack.pop_back();
                break;
            default:
                if (!lsystem->rotations.contains(c)) break;
                t.d = lsystem->rotations[c] * t.d;
                break;
        }
    }

    return false;
}

uint32_t VoxelGenerator::count_terminals(const std::shared_ptr<LSystem> &ls) {
    uint32_t count = 0;

    for (auto &val: ls->rules | views::values) {
        for (char c : val) {
            if (c == 'X' || c == 'F') {
                count++;
            }
        }
    }

    return count;
}
