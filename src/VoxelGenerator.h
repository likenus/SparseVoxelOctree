//
// Created by linus on 08.11.2025.
//

#ifndef SPARSEVOXELOCTREE_VOXELGENERATOR_H
#define SPARSEVOXELOCTREE_VOXELGENERATOR_H

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Timer.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "myvk/Buffer.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/CommandPool.hpp"
#include "myvk/ComputePipeline.hpp"
#include "myvk/DescriptorPool.hpp"
#include "myvk/DescriptorSet.hpp"
#include "myvk/PipelineLayout.hpp"



using namespace std;
using namespace glm;
class VoxelGenerator {
private:

    std::string m_sequence, m_axiom;
    uint32_t m_depth, m_voxel_resolution, m_num_terminals;

    shared_ptr<myvk::Device> m_device;
    vector<shared_ptr<myvk::CommandBuffer>> m_swap_command_buffer;
    vector<shared_ptr<myvk::Buffer>> m_swap_voxel_fragment_staging_buffer;
    vector<shared_ptr<myvk::Buffer>> m_swap_voxel_fragment_buffer;
    vector<shared_ptr<myvk::Fence>> m_swap_fence;

    void create_buffers(const std::shared_ptr<myvk::Device> &device);

    void create_descriptors(const std::shared_ptr<myvk::Device> &device);

    void create_pipeline(const std::shared_ptr<myvk::Device> &device);

    static uint32_t count_terminals(std::string axiom);

    uint32_t estimate_fragment_count() const;

    float scale = 1.f;
    float step = 1.f;
    float delta = 45.f * (M_PI / 180.f);

    struct AxiomInfo {
        uint32_t axiomLength, offset, depth, maxDepth, fragmentCount, num_terminals;
    };
    struct TurtleConstants {
        alignas(16) glm::vec3 _D, _N, _O; // direction, normal, origin
        float phi, step;
    };

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
    Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<Timer> &timer,
           const std::string &axiom, uint32_t depth);

    bool DoStep(glm::uvec2 &out_fragment);
    uint32_t GetLevel() const { return m_depth + 3; }
    uint32_t GetVoxelResolution() const { return m_voxel_resolution; }
    uint32_t GetVoxelFragmentCount() const { return m_num_terminals * estimate_fragment_count(); }
    const std::shared_ptr<myvk::Buffer> &GetVoxelFragmentList() const { return m_swap_voxel_fragment_buffer[0]; }
};

#endif //SPARSEVOXELOCTREE_VOXELGENERATOR_H
