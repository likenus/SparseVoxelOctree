//
// Created by linus on 08.11.2025.
//

#ifndef SPARSEVOXELOCTREE_VOXELGENERATOR_H
#define SPARSEVOXELOCTREE_VOXELGENERATOR_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "Timer.hpp"
#include "glm/glm.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/rotate_vector.hpp"


using namespace std;

class LSystem {
public:
    std::string axiom;
    std::unordered_map<char, std::string> rules;
    std::map<char, glm::mat3> rotations;
    std::set<char> rotation_commands = {'<', '>', '+', '-', '&', '|'};
    float delta = 0.f;
    char map_char;
    std::map<std::string, char> mapping;


    static std::shared_ptr<LSystem> Create(const std::string &ax, const float delta) {
        auto ret = std::make_shared<LSystem>();

        ret->delta = delta;
        ret->rotations = init_rotations(glm::radians(delta));

        // precompute composite rotations
        std::string axiom;
        std::map<std::string, char> mapping;
        std::string cur_rotation;
        glm::mat3 r(1.f);
        char map_char = 'a';

        for (char c : ax) {
            if (ret->rotation_commands.contains(c)) {
                r *= ret->rotations[c];
                cur_rotation += c;
            } else {
                if (!cur_rotation.empty()) {
                    if (!mapping.contains(cur_rotation)) {
                        ret->rotations[map_char] = r;
                        mapping[cur_rotation] = map_char;
                    }
                    axiom += map_char++;
                }
                cur_rotation = "";
                r = glm::mat3(1.f);
                axiom += c;
            }
        }
        if (!cur_rotation.empty()) {
            if (!mapping.contains(cur_rotation)) {
                ret->rotations[map_char] = r;
                mapping[cur_rotation] = map_char;
            }
            axiom += map_char++;
        }
        ret->axiom = axiom;
        ret->map_char = map_char;
        ret->mapping = mapping;

        return ret;
    }

    void addRule(const char symbol, const std::string& expansion) {

        std::string new_expansion;
        std::string cur_rotation;
        glm::mat3 r(1.f);
        for (char c : expansion) {
            if (rotation_commands.contains(c)) {
                r *= rotations[c];
                cur_rotation += c;
            } else {
                if (!cur_rotation.empty()) {
                    if (!mapping.contains(cur_rotation)) {
                        rotations[map_char] = r;
                        mapping[cur_rotation] = map_char;
                        new_expansion += map_char++;
                    } else {
                        new_expansion += mapping[cur_rotation];
                    }
                }
                cur_rotation = "";
                r = glm::mat3(1.f);
                new_expansion += c;
            }
        }
        if (!cur_rotation.empty()) {
            if (!mapping.contains(cur_rotation)) {
                rotations[map_char] = r;
                mapping[cur_rotation] = map_char;
            }
            new_expansion += map_char++;
        }
        rules[symbol] = new_expansion;
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

private:
    static std::map<char, glm::mat3> init_rotations(const float delta) {
        using namespace glm;
        std::map<char, mat3> rotations;
        rotations['+'] = mat3(
            cos(delta), -sin(delta), 0,
            sin(delta), cos(delta), 0,
            0, 0, 1);
        rotations['-'] = mat3(
            cos(-delta), -sin(-delta), 0,
            sin(-delta), cos(-delta), 0,
            0, 0, 1);
        rotations['<'] = mat3(
            cos(delta), 0, sin(delta),
            0, 1, 0,
            -sin(delta), 0, cos(delta));
        rotations['>'] = mat3(
            cos(-delta), 0, sin(-delta),
            0, 1, 0,
            -sin(-delta), 0, cos(-delta));
        rotations['&'] = mat3(
            1, 0, 0,
            0, cos(delta), sin(delta),
            0, -sin(delta), cos(delta));
        rotations['|'] = mat3(
            1, 0, 0,
            0, cos(-delta), sin(-delta),
            0, -sin(-delta), cos(-delta));

        return rotations;
    }
};

class VoxelGenerator {
private:
    struct TurtleState {
        glm::vec3 d, n, p;
    };

    uint32_t m_depth, m_octree_level, m_voxel_resolution, m_num_terminals;
    float step = 1.f;
    float delta = 45.f * (M_PI / 180.f);
    float sin_delta, cos_delta;
    uint32_t m_pos = 0; // pos in sequence
    TurtleState t{{0.f, 1.f, 0.f},
                  {0.f, 0.f, -1.f},
                  {.5f, .0f, .5f}};
    std::vector<TurtleState> stack;
    std::string m_sequence;
    std::shared_ptr<LSystem> lsystem;

    static uint32_t count_terminals(const std::shared_ptr<LSystem> &ls);

    uint32_t estimate_fragment_count() const;



public:

    bool is_done = false;

    static std::shared_ptr<VoxelGenerator> Create(
        const std::shared_ptr<Timer> &timer,
        const std::shared_ptr<LSystem> &ls,
        uint32_t octree_level,
        uint32_t depth);

    bool DoStep(glm::uvec4 &out_pos);
    uint32_t GetLevel() const { return m_octree_level; }
    uint32_t GetVoxelResolution() const { return m_voxel_resolution; }
    uint32_t GetVoxelFragmentCount() const { return m_num_terminals * estimate_fragment_count(); }
};

#endif //SPARSEVOXELOCTREE_VOXELGENERATOR_H
