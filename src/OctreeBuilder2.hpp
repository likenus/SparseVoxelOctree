//
// Created by Linus on 09.12.2025.
//

#ifndef SPARSEVOXELOCTREE_OCTREEBUILDER2_HPP
#define SPARSEVOXELOCTREE_OCTREEBUILDER2_HPP


#include "Counter.hpp"
#include "VoxelGenerator.h"

#include "myvk/Buffer.hpp"
#include "myvk/ComputePipeline.hpp"
#include "myvk/DescriptorSet.hpp"
#include "myvk/Framebuffer.hpp"
#include "myvk/RenderPass.hpp"

class OctreeBuilder2 {
private:
	std::shared_ptr<VoxelGenerator> m_voxel_generator_ptr;

	std::shared_ptr<myvk::Device> m_device;
	std::shared_ptr<myvk::CommandBuffer> m_command_buffer;
	std::shared_ptr<myvk::Fence> m_fence;
	std::shared_ptr<myvk::PipelineLayout> m_pipeline_layout;
	std::shared_ptr<myvk::ComputePipeline> m_tag_node_pipeline, m_init_node_pipeline, m_alloc_node_pipeline,
	    m_modify_arg_pipeline;

	Counter m_atomic_counter;

	std::shared_ptr<myvk::Buffer> m_octree_buffer, m_voxel_fragment_buffer;
	std::shared_ptr<myvk::Buffer> m_build_info_buffer, m_build_info_staging_buffer;
	std::shared_ptr<myvk::Buffer> m_indirect_buffer, m_indirect_staging_buffer;

	std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
	std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
	std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;

	std::vector<glm::uvec2> m_voxel_fragments;

	std::shared_ptr<Timer> m_timer;

	void create_buffers(const std::shared_ptr<myvk::Device> &device);
	void create_descriptors(const std::shared_ptr<myvk::Device> &device);
	void create_pipeline(const std::shared_ptr<myvk::Device> &device);
	void generate_fragments();

public:
	static std::shared_ptr<OctreeBuilder2> Create(const std::shared_ptr<VoxelGenerator> &generator,
	                                             const std::shared_ptr<myvk::CommandPool> &command_pool,
	                                             const std::shared_ptr<Timer> &timer);
	uint32_t GetLevel() const { return m_voxel_generator_ptr->GetLevel(); }

	void CmdBuild(const std::shared_ptr<myvk::CommandBuffer> &command_buffer) const;
	VkDeviceSize GetOctreeRange(const std::shared_ptr<myvk::CommandPool> &command_pool) const;
	const std::shared_ptr<myvk::Buffer> &GetOctree() const { return m_octree_buffer; }

	void CmdTransferOctreeOwnership(const std::shared_ptr<myvk::CommandBuffer> &command_buffer,
	                                uint32_t src_queue_family, uint32_t dst_queue_family,
	                                VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	                                VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) const;
	void Build();
	void DumpBuffer(const std::shared_ptr<myvk::CommandPool> &command_pool);
};



#endif //SPARSEVOXELOCTREE_OCTREEBUILDER2_HPP