//
// Created by Linus on 10.11.2025.
//

#include "DynamicOctree.hpp"

#include "glm/vec3.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/CommandPool.hpp"
#include "spdlog/spdlog.h"

std::shared_ptr<DynamicOctree> DynamicOctree::Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::Queue> &graphics_queue) {
    std::shared_ptr<DynamicOctree> ret = std::make_shared<DynamicOctree>();

    // TODO: Some constants to communicate buffer size?
    {
        VkDescriptorSetLayoutBinding dynamic_octree_binding = {};
        dynamic_octree_binding.binding = 0;
        dynamic_octree_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dynamic_octree_binding.descriptorCount = 1;
        dynamic_octree_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        ret->m_descriptor_set_layout = myvk::DescriptorSetLayout::Create(device, {dynamic_octree_binding});
    }

    ret->m_descriptor_pool = myvk::DescriptorPool::Create(device, 1, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}});
    ret->m_descriptor_set = myvk::DescriptorSet::Create(ret->m_descriptor_pool, ret->m_descriptor_set_layout); // TODO make multiple ?

    ret->CreateBuffers(device, graphics_queue);

    spdlog::info("initialized octree");

    return ret;
}

void DynamicOctree::CreateBuffers(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::Queue> &graphics_queue) {
    // Hardcoded cube
    std::vector<glm::vec3> data = {{glm::vec3(0.5f, 0.5f, 0.5f)}};
    size_t data_size = data.size() * sizeof(glm::vec3);

    spdlog::info("initializing voxel buffer");
    m_voxel_buffer = myvk::Buffer::Create(device, data_size, 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    std::shared_ptr<myvk::Buffer> voxel_staging_buffer = myvk::Buffer::CreateStaging(device, data.begin(), data.end());

    std::shared_ptr<myvk::Fence> fence = myvk::Fence::Create(device);
    std::shared_ptr<myvk::CommandPool> command_pool = myvk::CommandPool::Create(graphics_queue);
    std::shared_ptr<myvk::CommandBuffer> command_buffer = myvk::CommandBuffer::Create(command_pool);
    command_buffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    command_buffer->CmdCopy(voxel_staging_buffer, m_voxel_buffer, {{0, 0, data_size}});
    command_buffer->End();
    command_buffer->Submit(fence);

    fence->Wait();

    spdlog::info("finished initializing buffer");

    m_descriptor_set->UpdateStorageBuffer(m_voxel_buffer, 0, 0, 0, data_size);
}


// TODO: Dynamic Octree Logic, preferably as compute shader
