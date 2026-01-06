//
// Created by linus on 27.12.2025.
//

#ifndef SPARSEVOXELOCTREE_OCTREEBUILDER3_HPP
#define SPARSEVOXELOCTREE_OCTREEBUILDER3_HPP

#include "Counter.hpp"
#include "StackAllocator.hpp"
#include "VoxelGenerator.h"

#include "myvk/Buffer.hpp"
#include "myvk/ComputePipeline.hpp"
#include "myvk/DescriptorSet.hpp"
#include "myvk/Framebuffer.hpp"
#include "myvk/RenderPass.hpp"

/*
 * batch_size | duration
 * 8.192        660 ms
 * 16.384       552 ms
 * 32.768       593 ms
*/
const uint32_t BATCH_SIZE = 1 << 13;
const uint32_t SWAP_COUNT = 2;

using namespace std;
using namespace glm;

class OctreeBuilder3 {
private:
    std::shared_ptr<VoxelGenerator> m_voxel_generator_ptr;

    shared_ptr<myvk::Device> m_device;
    std::shared_ptr<myvk::PipelineLayout> m_pipeline_layout;
    std::shared_ptr<myvk::ComputePipeline> m_tag_node_pipeline, m_init_node_pipeline, m_alloc_node_pipeline,
            m_modify_arg_pipeline, m_reset_buffer_pipeline;

    Counter m_atomic_counter;

    std::shared_ptr<myvk::Buffer> m_octree_buffer, m_voxel_fragment_buffer, m_tag_alloc_buffer;
    std::shared_ptr<myvk::Buffer> m_build_info_buffer, m_build_info_staging_buffer;
    std::shared_ptr<myvk::Buffer> m_indirect_buffer, m_indirect_staging_buffer;

    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;

    vector<vector<glm::uvec2>> m_swap_fragment_list = vector<vector<uvec2>>(SWAP_COUNT, vector<uvec2>(BATCH_SIZE));
    shared_ptr<myvk::CommandBuffer> m_command_buffer;
    vector<shared_ptr<myvk::Buffer>> m_swap_voxel_fragment_staging_buffer = vector<shared_ptr<myvk::Buffer>>(SWAP_COUNT);
    shared_ptr<myvk::Fence> m_fence;

    shared_ptr<StackAllocator> m_stack_allocator;

    void create_buffers(const std::shared_ptr<myvk::Device> &device);
    void create_descriptors(const std::shared_ptr<myvk::Device> &device);
    void create_pipeline(const std::shared_ptr<myvk::Device> &device);
    void create_build_resources(const shared_ptr<myvk::Device> &device, const shared_ptr<myvk::CommandPool> &command_pool);

public:
    static std::shared_ptr<OctreeBuilder3> Create(const std::shared_ptr<VoxelGenerator> &generator,
                                                  const std::shared_ptr<myvk::CommandPool> &command_pool,
                                                  const std::shared_ptr<Timer> &timer);
    uint32_t GetLevel() const { return m_voxel_generator_ptr->GetLevel(); }

    void CmdInit(const std::shared_ptr<myvk::CommandBuffer> &command_buffer) const;
    void CmdBuild(const std::shared_ptr<myvk::CommandBuffer> &command_buffer) const;
    void Build();
    VkDeviceSize GetOctreeRange(const std::shared_ptr<myvk::CommandPool> &command_pool) const;
    const std::shared_ptr<myvk::Buffer> &GetOctree() const { return m_octree_buffer; }

    void CmdTransferOctreeOwnership(const std::shared_ptr<myvk::CommandBuffer> &command_buffer,
                                    uint32_t src_queue_family, uint32_t dst_queue_family,
                                    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) const;

    void DumpBuffer(const std::shared_ptr<myvk::CommandPool> &command_pool);
};


#endif //SPARSEVOXELOCTREE_OCTREEBUILDER3_HPP
