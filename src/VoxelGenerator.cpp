//
// Created by linus on 08.11.2025.
//

#include "VoxelGenerator.h"

#include <unordered_map>

#include "glm/gtx/rotate_vector.hpp"
#include "spdlog/spdlog.h"


class LSystem {
public:
    std::string axiom;
    std::unordered_map<char, std::string> rules;

    LSystem(const std::string& ax) : axiom(ax) {}

    void addRule(char symbol, const std::string& expansion) {
        rules[symbol] = expansion;
    }

    std::string iterate(const uint32_t depth) {
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
        return result;
    }
};

std::shared_ptr<VoxelGenerator> VoxelGenerator::Create(const std::string &sequence, const uint32_t depth, const float scale) {
    auto ret = std::make_shared<VoxelGenerator>();

    LSystem ls("F");
    ls.addRule('F', sequence);
    ret->m_sequence = ls.iterate(depth);
    ret->scale = scale;
    ret->step = 1 / scale;

    return ret;
}

bool VoxelGenerator::DoStep(glm::vec4 &out_pos) {
    if (m_pos >= m_sequence.length()) {
        is_done = true;
        return false; // Potentially grow
    }

    switch (m_sequence[m_pos++]) {
        case 'F': {
            t.p += t.d * step;

            glm::vec4 pos = glm::vec4(std::round(t.p.x * scale) / scale, std::round(t.p.y * scale) / scale, std::round(t.p.z * scale) / scale, 0.f) + (step / 2.f);
            out_pos = pos;
            m_voxel_data.push_back(pos);
            return true;
        }

        case '+': t.d = glm::rotate(t.d, angle, t.n); break;
        case '-': t.d = glm::rotate(t.d, -angle, t.n); break;
        case '<': t.n = glm::rotate(t.n, angle, t.d); break;
        case '>': t.n = glm::rotate(t.n, -angle, t.d); break;

        case '[':
            stack.push_back(t);break;
        case ']':
            t = stack.back();
            stack.pop_back();
            break;
        default:
            break;
    }

    return false;
}
