//
// Created by linus on 08.11.2025.
//

#include "VoxelGenerator.h"
#include <unordered_map>
#include <chrono>
#include <iostream>

#include "Quaternion.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/Fence.hpp"
#include "spdlog/spdlog.h"

#define COLOR_MASK 0x00ffffffu


std::shared_ptr<VoxelGenerator> VoxelGenerator::Create(const std::shared_ptr<Timer> &timer,
    const std::string &axiom, const uint32_t depth) {
    auto ret = std::make_shared<VoxelGenerator>();

    auto ls = LSystem::Create("F");
    ls->addRule('F', axiom);

    timer->new_lap();
    ret->m_sequence = ls->iterate(std::max(1u, depth - 3));
    timer->lap("iterating string");

    ret->m_voxel_resolution = 1u << depth;
    ret->step = 1.f / static_cast<float>(ret->m_voxel_resolution);
    ret->m_octree_level = depth;
    ret->m_depth = depth - 3;
    ret->delta = 22.5f * (M_PI / 180.f);
    ret->m_num_terminals = count_terminals(ls);

    return ret;
}

std::shared_ptr<VoxelGenerator> VoxelGenerator::Create(
    const std::shared_ptr<Timer> &timer,
    const std::shared_ptr<LSystem> &ls,
    uint32_t octree_level,
    const uint32_t depth,
    const float delta) {

    auto ret = std::make_shared<VoxelGenerator>();
    ret->m_sequence = ls->iterate(std::max(1u, depth));
    ret->m_voxel_resolution = 1u << octree_level;
    ret->step = 1.f / static_cast<float>(ret->m_voxel_resolution);
    ret->m_octree_level = octree_level;
    ret->m_depth = depth;
    ret->delta = delta * static_cast<float>(M_PI / 180.f);
    ret->m_num_terminals = count_terminals(ls);

    return ret;
}


uint32_t VoxelGenerator::estimate_fragment_count() const {
    return std::pow(m_num_terminals, m_depth);
}

bool VoxelGenerator::DoStep(glm::uvec2 &out_fragment) {
    if (m_pos >= m_sequence.length()) {
        is_done = true;
        return false; // Potentially grow
    }

    while (m_pos < m_sequence.length()) {

        switch (m_sequence[m_pos++]) {
            case 'F': {
                t.p += t.d * step;
                glm::uvec2 fragment;
                auto x = static_cast<uint32_t>(std::round(t.p.x * m_voxel_resolution));
                auto y = static_cast<uint32_t>(std::round(t.p.y * m_voxel_resolution));
                auto z = static_cast<uint32_t>(std::round(t.p.z * m_voxel_resolution));

                uint32_t color = 0x00092a3b;
                if (m_pos == m_sequence.size() || m_sequence[m_pos] == ']') {
                    color = 0x00043d0e;
                }

                fragment.x = x | (y << 12) | ((z & 0xff) << 24);
                fragment.y = ((z >> 8) << 28) | COLOR_MASK & color;
                out_fragment = fragment;
                return true;
            }
            case '+': t.d = Quaternion::rotate(t.d, -delta, t.n); break;
            case '-': t.d = Quaternion::rotate(t.d, delta, t.n); break;
            case '<': t.n = Quaternion::rotate(t.n, -delta, t.d); break;
            case '>': t.n = Quaternion::rotate(t.n, delta, t.d); break;

            case '[':
                stack.push_back(t); break;
            case ']':
                t = stack.back();
                stack.pop_back();
                break;
            default:
                break;
        }
    }

    return false;
}

uint32_t VoxelGenerator::count_terminals(const std::shared_ptr<LSystem> &ls) {
    uint32_t count = 0;

    for (auto &[key, val] : ls->rules) {
        for (char c : val) {
            if (c == 'X' || c == 'F') {
                count++;
            }
        }
    }

    return count;
}


