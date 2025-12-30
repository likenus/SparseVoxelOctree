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

class LSystem {
public:
    std::string axiom;
    std::unordered_map<char, std::string> rules;
    std::shared_ptr<Timer> m_timer;

    LSystem(const std::string& ax, const std::shared_ptr<Timer> &timer) : axiom(ax), m_timer(timer) {}

    void addRule(char symbol, const std::string& expansion) {
        rules[symbol] = expansion;
    }

    std::string iterate(const uint32_t depth) {
        m_timer->new_lap();
        std::string result = axiom;

        for (int i = 0; i < depth; i++) {
            std::string next;
            for (char c : result) {
                if (rules.contains(c))
                    next += rules[c];
                else
                    next += c;
            }
            result = next;
        }
        m_timer->lap("iterating string");
        return result;
    }
};


std::shared_ptr<VoxelGenerator> VoxelGenerator::Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<Timer> &timer,
    const std::string &axiom, const uint32_t depth) {
    auto ret = std::make_shared<VoxelGenerator>();

    LSystem ls("F", timer);
    ls.addRule('F', axiom);
    ret->m_axiom = axiom;
    ret->m_sequence = ls.iterate(std::max(1u, depth - 3));
    ret->scale = std::pow(2.f, static_cast<float>(depth));
    ret->step = 1 / ret->scale;
    ret->m_depth = depth - 3;
    ret->m_voxel_resolution = 1u << depth;
    ret->delta = 22.5f * (M_PI / 180.f);
    ret->m_num_terminals = count_terminals(axiom);
    ret->m_device = device;

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
                glm::vec4 pos = glm::vec4(std::round(t.p.x * scale) / scale, std::round(t.p.y * scale) / scale, std::round(t.p.z * scale) / scale, 0.f) + (step / 2.f);
                glm::uvec2 fragment;
                uint32_t x, y, z;
                x = static_cast<uint32_t>(pos.x * m_voxel_resolution);
                y = static_cast<uint32_t>(pos.y * m_voxel_resolution);
                z = static_cast<uint32_t>(pos.z * m_voxel_resolution);

                uint32_t color = 0x00092a3b;
                if (m_pos == m_sequence.size() || m_sequence[m_pos] == ']') {
                    color = 0x00043d0e;
                }

                fragment.x = x | (y << 12) | ((z & 0xff) << 24);
                fragment.y = ((z >> 8) << 28) | COLOR_MASK & color;
                //m_fragment_list.push_back(fragment);
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

uint32_t VoxelGenerator::count_terminals(std::string axiom) {
    uint32_t count = 0;

    for (char c: axiom) {
        count += (c == 'F');
    }

    return count;
}


