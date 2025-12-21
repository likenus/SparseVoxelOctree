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
    std::vector<uint32_t> m_long_axiom;
    uint32_t m_depth, m_voxel_resolution, m_num_terminals;

    std::shared_ptr<myvk::Device> m_device;
    std::shared_ptr<myvk::PipelineLayout> m_pipeline_layout;
    std::shared_ptr<myvk::ComputePipeline> m_generator_generate_pipeline, m_generator_iterate_pipeline, m_generator_modify_arg_pipeline;
    std::shared_ptr<myvk::Buffer> m_axiom_buffer, m_axiom_info_buffer, m_turtle_constant_buffer,
    m_translation_buffer, m_rotation_buffer, m_voxel_fragment_buffer,
        m_axiom_staging_buffer, m_axiom_info_staging_buffer, m_turtle_constants_staging_buffer,
        m_rotation_staging_buffer, m_translation_staging_buffer, m_debug_buffer;

    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;

    void create_buffers(const std::shared_ptr<myvk::Device> &device);
    void create_descriptors(const std::shared_ptr<myvk::Device> &device);
    void create_pipeline(const std::shared_ptr<myvk::Device> &device);
    static uint32_t count_terminals(std::string axiom);

    uint32_t estimate_fragment_count() const;

    float scale = 1.f;
    float step = 1.f;
    float delta = 90.f * (M_PI / 360.f);

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
    TurtleState t{{0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {.5f, .0f, .5f}};
    uint32_t m_pos = 0; // pos in sequence

public:
    bool is_done = false;
    static std::shared_ptr<VoxelGenerator> Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::CommandPool> &command_pool,
        const std::string &axiom, uint32_t depth);
    bool DoStep(glm::vec4 &out_pos);
    void Generate();
    uint32_t GetLevel() const { return m_depth + 3; }
    uint32_t GetVoxelResolution() const { return m_voxel_resolution; }
    uint32_t GetVoxelFragmentCount() const { return m_num_terminals * estimate_fragment_count(); }
    const std::shared_ptr<myvk::Buffer> &GetVoxelFragmentList() const { return m_voxel_fragment_buffer; }
    void CmdGenerate(std::shared_ptr<myvk::CommandBuffer> &command_buffer);
    void DumpBuffer();
};


#endif //SPARSEVOXELOCTREE_VOXELGENERATOR_H
