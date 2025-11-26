//
// Created by linus on 08.11.2025.
//

#ifndef SPARSEVOXELOCTREE_VOXELGENERATOR_H
#define SPARSEVOXELOCTREE_VOXELGENERATOR_H

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "glm/vec3.hpp"
#include "glm/vec4.hpp"


class VoxelGenerator {
private:
    std::vector<glm::vec4> m_voxel_data;
    std::string m_sequence;

    float scale;
    float step;
    float angle = 25.f * (M_PI / 180.f);


    struct TurtleState {
        glm::vec3 d, n, p;
    };
    std::vector<TurtleState> stack;
    TurtleState t{{0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {1.5f, 1.f, 1.5f}};
    uint32_t m_pos = 0; // pos in sequence

public:
    bool is_done = false;
    static std::shared_ptr<VoxelGenerator> Create(const std::string &sequence, uint32_t depth, float scale);
    std::vector<glm::vec4> &GetVoxelData() { return m_voxel_data; };
    bool DoStep(glm::vec4 &out_pos);
};


#endif //SPARSEVOXELOCTREE_VOXELGENERATOR_H
