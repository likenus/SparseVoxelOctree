//
// Created by linus on 08.11.2025.
//

#ifndef SPARSEVOXELOCTREE_VOXELGENERATOR_H
#define SPARSEVOXELOCTREE_VOXELGENERATOR_H

#include <memory>
#include <string>
#include <vector>

#include "Timer.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"


using namespace std;

class LSystem {
public:
    std::string axiom;
    std::unordered_map<char, std::string> rules;


    static std::shared_ptr<LSystem> Create(const std::string &ax) {
        auto ret = std::make_shared<LSystem>();

        ret->axiom = ax;

        return ret;
    }

    void addRule(const char symbol, const std::string& expansion) {
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

class VoxelGenerator {
private:

    std::string m_sequence;
    uint32_t m_depth, m_octree_level, m_voxel_resolution, m_num_terminals;

    static uint32_t count_terminals(const std::shared_ptr<LSystem> &ls);

    uint32_t estimate_fragment_count() const;

    float step = 1.f;
    float delta = 45.f * (M_PI / 180.f);

    struct TurtleState {
        glm::vec3 d, n, p;
    };
    std::vector<TurtleState> stack;
    TurtleState t{{0.f, 1.f, 0.f},
                  {0.f, 0.f, -1.f},
                  {.5f, .0f, .5f}};
    uint32_t m_pos = 0; // pos in sequence

public:

    bool is_done = false;

    static std::shared_ptr<VoxelGenerator>
    Create(const std::shared_ptr<Timer> &timer,
           const std::string &axiom, uint32_t depth);

    static std::shared_ptr<VoxelGenerator> Create(
        const std::shared_ptr<Timer> &timer,
        const std::shared_ptr<LSystem> &ls,
        uint32_t octree_level,
        uint32_t depth,
        float delta);

    bool DoStep(glm::uvec2 &out_fragment);
    uint32_t GetLevel() const { return m_octree_level; }
    uint32_t GetVoxelResolution() const { return m_voxel_resolution; }
    uint32_t GetVoxelFragmentCount() const { return m_num_terminals * estimate_fragment_count(); }
};

#endif //SPARSEVOXELOCTREE_VOXELGENERATOR_H
