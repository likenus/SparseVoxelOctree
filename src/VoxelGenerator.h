//
// Created by linus on 08.11.2025.
//

#ifndef SPARSEVOXELOCTREE_VOXELGENERATOR_H
#define SPARSEVOXELOCTREE_VOXELGENERATOR_H


#include <cmath>
#include <memory>
#include <string>
#include <vector>

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


class VoxelGenerator {
private:
    std::vector<glm::uvec2> m_fragment_list;
    std::string m_sequence, m_axiom;
    uint32_t depth, m_voxel_resolution;

    std::shared_ptr<myvk::PipelineLayout> m_pipeline_layout;
    std::shared_ptr<myvk::ComputePipeline> m_generator_generate_pipeline, m_generator_iterate_pipeline;
    std::shared_ptr<myvk::Buffer> m_voxel_fragment_buffer, m_axiom_buffer, m_axiom_info_buffer,
        m_turtle_constant_buffer, m_translation_buffer, m_rotation_buffer;

    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;

    void create_buffers(const std::shared_ptr<myvk::Device> &device);
    void create_descriptors(const std::shared_ptr<myvk::Device> &device);
    void create_pipeline(const std::shared_ptr<myvk::Device> &device);

    uint32_t estimate_fragment_count() const;

    float scale;
    float step;
    float delta = 22.5f * (M_PI / 180.f);

    struct TurtleConstants {
        alignas(16) glm::vec3 _D, _N, _P, _O; // direction, normal, D x N, origin
        float phi, step;
    };

    struct TurtleState {
        glm::vec3 d, n, p;
    };
    std::vector<TurtleState> stack;
    TurtleState t{{0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {.5f, .0f, .5f}};
    uint32_t m_pos = 0; // pos in sequence

public:
    bool is_done = false;
    static std::shared_ptr<VoxelGenerator> Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::CommandPool> &command_pool,
        const std::string &axiom, uint32_t depth);
    bool DoStep(glm::vec4 &out_pos);
    void Generate(const std::shared_ptr<myvk::CommandPool> &command_pool);
    uint32_t GetDepth() const { return depth; }
    uint32_t GetVoxelResolution() const { return m_voxel_resolution; }
    uint32_t GetVoxelFragmentCount() const { return m_fragment_list.size(); }
    const std::shared_ptr<myvk::Buffer> &GetVoxelFragmentList() const { return m_voxel_fragment_buffer; }
    void CmdGenerate(std::shared_ptr<myvk::CommandBuffer> &command_buffer);
};

#endif //SPARSEVOXELOCTREE_VOXELGENERATOR_H
