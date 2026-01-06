//
// Created by linus on 27.12.2025.
//

#include "OctreeBuilder3.hpp"

#include "Config.hpp"

#include <spdlog/spdlog.h>

inline static constexpr uint32_t group_x_64(uint32_t x) { return (x >> 6u) + ((x & 0x3fu) ? 1u : 0u); }

std::shared_ptr<OctreeBuilder3> OctreeBuilder3::Create(const std::shared_ptr<VoxelGenerator> &generator,
                                                       const std::shared_ptr<myvk::CommandPool> &command_pool,
                                                       const std::shared_ptr<Timer> &timer) {
    std::shared_ptr<OctreeBuilder3> ret = std::make_shared<OctreeBuilder3>();

    auto device = command_pool->GetDevicePtr();
    ret->m_device = command_pool->GetDevicePtr();
    ret->m_voxel_generator_ptr = generator;
    ret->m_atomic_counter.Initialize(ret->m_device);
    ret->m_atomic_counter.Reset(command_pool, 0);

    {
        uint32_t octree_node_ratio = generator->GetLevel();
        uint32_t octree_entry_num =
                std::max(kOctreeNodeNumMin, generator->GetVoxelFragmentCount() * octree_node_ratio);
        octree_entry_num = std::min(octree_entry_num, kOctreeNodeNumMax);
        ret->m_stack_allocator = StackAllocator::Create(device, command_pool, octree_entry_num);
    }

    ret->create_buffers(ret->m_device);
    ret->create_descriptors(ret->m_device);
    ret->create_pipeline(ret->m_device);
    ret->create_build_resources(ret->m_device, command_pool);

    return ret;
}

void OctreeBuilder3::create_buffers(const std::shared_ptr<myvk::Device> &device) {
    m_build_info_buffer = myvk::Buffer::Create(device, 3 * sizeof(uint32_t), 0,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_build_info_staging_buffer = myvk::Buffer::CreateStaging<uint32_t>(device, 5, [this](uint32_t *data) {
        data[0] = 0; // uAllocBegin
        data[1] = 8; // uAllocNum
        data[2] = BATCH_SIZE; // uBatchSize
        data[3] = m_voxel_generator_ptr->GetLevel();
        data[4] = m_voxel_generator_ptr->GetLevel();
    });

    m_indirect_buffer = myvk::Buffer::Create(device, 3 * sizeof(uint32_t), 0,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_indirect_staging_buffer = myvk::Buffer::CreateStaging<uint32_t>(device, 3, [](uint32_t *data) {
        data[0] = 1; // uGroupX
        data[1] = 1; // uGroupY
        data[2] = 1; // uGroupZ
    });
    m_voxel_fragment_buffer = myvk::Buffer::Create(device, BATCH_SIZE * sizeof(uvec2), 0,
                                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    m_tag_alloc_buffer = myvk::Buffer::Create(device, BATCH_SIZE * sizeof(uint32_t) * 4, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Estimate octree buffer size
    uint32_t octree_node_ratio = m_voxel_generator_ptr->GetLevel() / 4;
    uint32_t octree_entry_num =
            std::max(kOctreeNodeNumMin, m_voxel_generator_ptr->GetVoxelFragmentCount() * octree_node_ratio);
    octree_entry_num = std::min(octree_entry_num, kOctreeNodeNumMax);

    m_octree_buffer =
            myvk::Buffer::Create(device, octree_entry_num * sizeof(uint32_t), 0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    spdlog::info("Octree buffer created with {} nodes ({} MB)", octree_entry_num,
                 m_octree_buffer->GetSize() / 1000000.0);
}

void OctreeBuilder3::create_descriptors(const std::shared_ptr<myvk::Device> &device) {
    m_descriptor_pool = myvk::DescriptorPool::Create(device, 1, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6}});
    {
        VkDescriptorSetLayoutBinding atomic_counter_binding = {};
        atomic_counter_binding.binding = 0;
        atomic_counter_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        atomic_counter_binding.descriptorCount = 1;
        atomic_counter_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding octree_binding = {};
        octree_binding.binding = 1;
        octree_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        octree_binding.descriptorCount = 1;
        octree_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding fragment_list_binding = {};
        fragment_list_binding.binding = 2;
        fragment_list_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        fragment_list_binding.descriptorCount = 1;
        fragment_list_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding build_info_binding = {};
        build_info_binding.binding = 3;
        build_info_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        build_info_binding.descriptorCount = 1;
        build_info_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding indirect_binding = {};
        indirect_binding.binding = 4;
        indirect_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        indirect_binding.descriptorCount = 1;
        indirect_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding tag_alloc_binding = {};
        tag_alloc_binding.binding = 5;
        tag_alloc_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        tag_alloc_binding.descriptorCount = 1;
        tag_alloc_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        m_descriptor_set_layout =
                myvk::DescriptorSetLayout::Create(device, {atomic_counter_binding, octree_binding, fragment_list_binding,
                                                           build_info_binding, indirect_binding, tag_alloc_binding});
    }
    m_descriptor_set = myvk::DescriptorSet::Create(m_descriptor_pool, m_descriptor_set_layout);
    m_descriptor_set->UpdateStorageBuffer(m_atomic_counter.GetBuffer(), 0);
    m_descriptor_set->UpdateStorageBuffer(m_octree_buffer, 1);
    m_descriptor_set->UpdateStorageBuffer(m_voxel_fragment_buffer, 2);
    m_descriptor_set->UpdateStorageBuffer(m_build_info_buffer, 3);
    m_descriptor_set->UpdateStorageBuffer(m_indirect_buffer, 4);
    m_descriptor_set->UpdateStorageBuffer(m_tag_alloc_buffer, 5);
}

void OctreeBuilder3::create_pipeline(const std::shared_ptr<myvk::Device> &device) {
    m_pipeline_layout = myvk::PipelineLayout::Create(device, {m_descriptor_set_layout, m_stack_allocator->GetDescriptorSetLayout()}, {});

    {
        uint32_t spec_data[] = {m_voxel_generator_ptr->GetVoxelResolution(), BATCH_SIZE}; // kVoxelResolution and kFragmentCount
        VkSpecializationMapEntry spec_entries[] = {{0, 0, sizeof(uint32_t)}, {1, sizeof(uint32_t), sizeof(uint32_t)}};
        VkSpecializationInfo spec_info = {2, spec_entries, 2 * sizeof(uint32_t), spec_data};
        constexpr uint32_t kOctreeTagNodeCompSpv[] = {
#include "spirv/octree_tag_node2.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> octree_tag_node_shader_module =
                myvk::ShaderModule::Create(device, kOctreeTagNodeCompSpv, sizeof(kOctreeTagNodeCompSpv));
        m_tag_node_pipeline =
                myvk::ComputePipeline::Create(m_pipeline_layout, octree_tag_node_shader_module, &spec_info);
    }

    {
        constexpr uint32_t kOctreeInitNodeCompSpv[] = {
#include "spirv/octree_init_node2.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> octree_init_node_shader_module =
                myvk::ShaderModule::Create(device, kOctreeInitNodeCompSpv, sizeof(kOctreeInitNodeCompSpv));
        m_init_node_pipeline = myvk::ComputePipeline::Create(m_pipeline_layout, octree_init_node_shader_module);
    }

    {
        constexpr uint32_t kOctreeAllocNodeCompSpv[] = {
#include "spirv/octree_alloc_node2.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> octree_alloc_node_shader_module =
                myvk::ShaderModule::Create(device, kOctreeAllocNodeCompSpv, sizeof(kOctreeAllocNodeCompSpv));
        m_alloc_node_pipeline = myvk::ComputePipeline::Create(m_pipeline_layout, octree_alloc_node_shader_module);
    }

    {
        constexpr uint32_t kOctreeModifyArgCompSpv[] = {
#include "spirv/octree_modify_arg2.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> octree_modify_arg_shader_module =
                myvk::ShaderModule::Create(device, kOctreeModifyArgCompSpv, sizeof(kOctreeModifyArgCompSpv));
        m_modify_arg_pipeline = myvk::ComputePipeline::Create(m_pipeline_layout, octree_modify_arg_shader_module);
    }

    {
        constexpr uint32_t kOctreeResetBufferCompSpv[] = {
#include "spirv/octree_reset_buffer.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> octree_reset_buffer_shader_module =
            myvk::ShaderModule::Create(device, kOctreeResetBufferCompSpv, sizeof(kOctreeResetBufferCompSpv));
        m_reset_buffer_pipeline = myvk::ComputePipeline::Create(m_pipeline_layout, octree_reset_buffer_shader_module);
    }
}

void OctreeBuilder3::CmdInit(const std::shared_ptr<myvk::CommandBuffer> &command_buffer) const {
    command_buffer->CmdBindDescriptorSets({m_descriptor_set}, m_pipeline_layout, VK_PIPELINE_BIND_POINT_COMPUTE, {});
    command_buffer->CmdCopy(m_build_info_staging_buffer, m_build_info_buffer,
                                {{0, 0, m_build_info_buffer->GetSize()}});
    command_buffer->CmdCopy(m_indirect_staging_buffer, m_indirect_buffer, {{0, 0, m_indirect_buffer->GetSize()}});

    command_buffer->CmdPipelineBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
            {m_build_info_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
            {});

    command_buffer->CmdPipelineBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            {},
            {m_indirect_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                 VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
            {});

    command_buffer->CmdBindPipeline(m_init_node_pipeline);
    command_buffer->CmdDispatchIndirect(m_indirect_buffer);
}

void OctreeBuilder3::CmdBuild(const std::shared_ptr<myvk::CommandBuffer> &command_buffer) const {

    uint32_t fragment_group_x = group_x_64(BATCH_SIZE);

    command_buffer->CmdBindDescriptorSets({m_descriptor_set, m_stack_allocator->GetDescriptorSet()}, m_pipeline_layout, VK_PIPELINE_BIND_POINT_COMPUTE, {});

    command_buffer->CmdBindPipeline(m_reset_buffer_pipeline);
    command_buffer->CmdDispatch(fragment_group_x, 1, 1);

    command_buffer->CmdPipelineBarrier(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
        {m_tag_alloc_buffer->GetMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
        m_build_info_buffer->GetMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
            {});

    command_buffer->CmdBindPipeline(m_tag_node_pipeline);
    command_buffer->CmdDispatch(fragment_group_x, 1, 1);

}

VkDeviceSize OctreeBuilder3::GetOctreeRange(const std::shared_ptr<myvk::CommandPool> &command_pool) const {
    return m_stack_allocator->GetRange();
}
void OctreeBuilder3::CmdTransferOctreeOwnership(const std::shared_ptr<myvk::CommandBuffer> &command_buffer,
                                                uint32_t src_queue_family, uint32_t dst_queue_family,
                                                VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) const {
    command_buffer->CmdPipelineBarrier(
            src_stage, dst_stage, {}, {m_octree_buffer->GetMemoryBarrier(0, 0, src_queue_family, dst_queue_family)}, {});
}

void OctreeBuilder3::create_build_resources(const shared_ptr<myvk::Device> &device,
                                            const shared_ptr<myvk::CommandPool> &command_pool) {
    m_command_buffer = myvk::CommandBuffer::Create(command_pool);
    m_fence = myvk::Fence::Create(device, VK_FENCE_CREATE_SIGNALED_BIT);

    {
        shared_ptr<myvk::CommandBuffer> command_buffer = myvk::CommandBuffer::Create(command_pool);
        shared_ptr<myvk::Fence> fence = myvk::Fence::Create(device);
        command_buffer->Begin();
        CmdInit(command_buffer);
        command_buffer->End();
        command_buffer->Submit(fence);
        fence->Wait();
    }
}

void OctreeBuilder3::Build() {

    uint32_t swap = 0;
    uvec2 voxel_fragment;
    int count = 0;
    while (!m_voxel_generator_ptr->is_done) {
        for (uint32_t i = 0; i < BATCH_SIZE; i++) {
            if (m_voxel_generator_ptr->DoStep(voxel_fragment)) [[likely]] {
                m_swap_fragment_list[swap][i] = voxel_fragment;
                count++;
            } else {
                m_swap_fragment_list[swap][i] = uvec2(0);
            }
        }

        m_swap_voxel_fragment_staging_buffer[swap] = myvk::Buffer::CreateStaging(m_device, m_swap_fragment_list[swap].begin(), m_swap_fragment_list[swap].end());

        m_fence->Wait(); // Wait for previous dispatch to finish
        m_fence->Reset();

        m_command_buffer->Reset();
        m_command_buffer->Begin();
        m_command_buffer->CmdCopy(m_swap_voxel_fragment_staging_buffer[swap], m_voxel_fragment_buffer,
                                             {{0, 0, m_swap_voxel_fragment_staging_buffer[swap]->GetSize()}});
        m_command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
                                                        {m_voxel_fragment_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
                                                        {});

        CmdBuild(m_command_buffer);
        m_command_buffer->End();
        m_command_buffer->Submit(m_fence);


        swap ^= 1;
    }

    m_fence->Wait(); // Wait for final dispatch to finish


    spdlog::info("Generated {} fragments", count);

    // PSEUDOCODE
    /*
     * until all voxels generated:
     *  Generate BATCH_SIZE voxel fragments
     *  wait for fence from previous iteration
     *  create and fill staging buffer
     *  dispatch cmd build
     * end while
     */
}

void OctreeBuilder3::DumpBuffer(const std::shared_ptr<myvk::CommandPool> &command_pool) {
    uint32_t *buffer_content = static_cast<uint32_t *>(m_octree_buffer->GetMappedData());

    uint32_t range = GetOctreeRange(command_pool) / sizeof(uint32_t);
    spdlog::info("Dumping buffer. {} entries", range);
    for (int i = 0; i < range; i++) {
        uint32_t cur = buffer_content[i];
        std::cout << i << ": ";
        if (cur & 0x40000000u) {
            std::cout << "Leaf" << std::endl;
        } else if (cur & 0x80000000u) {
            std::cout << ((cur & 0x3fffffff)) << std::endl;
        }
        else {
            std::cout << cur << std::endl;
        }
    }
}
