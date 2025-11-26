//
// Created by Linus on 10.11.2025.
//

#include "DynamicOctree.hpp"

#include "glm/vec3.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/CommandPool.hpp"
#include "spdlog/spdlog.h"

std::shared_ptr<DynamicOctree> DynamicOctree::Create(
    const std::shared_ptr<myvk::Device> &device,
    const std::shared_ptr<myvk::Queue> &graphics_queue) {
    std::shared_ptr<DynamicOctree> ret = std::make_shared<DynamicOctree>();

    ret->m_device = device;
    ret->m_graphics_queue = graphics_queue;

    {
        VkDescriptorSetLayoutBinding octree_info_binding = {};
        octree_info_binding.binding = 0;
        octree_info_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        octree_info_binding.descriptorCount = 1;
        octree_info_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding voxel_buffer_binding = {};
        voxel_buffer_binding.binding = 1;
        voxel_buffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        voxel_buffer_binding.descriptorCount = 1;
        voxel_buffer_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding octree_binding = {};
        octree_binding.binding = 2;
        octree_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        octree_binding.descriptorCount = 1;
        octree_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        ret->m_descriptor_set_layout = myvk::DescriptorSetLayout::Create(device, {octree_info_binding, voxel_buffer_binding, octree_binding});
    }

    ret->m_descriptor_pool = myvk::DescriptorPool::Create(device, 1,
        { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} });
    ret->m_descriptor_set = myvk::DescriptorSet::Create(ret->m_descriptor_pool, ret->m_descriptor_set_layout);

    ret->m_octree.resize(1 << 20);
    ret->m_octree[ret->m_root] = OctreeEntry{0x40000}; // Root Node with child pointer to address 2   0b ...010 0 00000000 00000000
    ret->m_voxel_generator = VoxelGenerator::Create("F[+F][>--F++<]F[-F][>++F--<]F", 4, std::pow(2.f, static_cast<float>(ret->m_octree_info.resolution - 1)));

    ret->CreateBuffers(device);


    spdlog::info("initialized octree");

    return ret;
}

void DynamicOctree::CreateBuffers(const std::shared_ptr<myvk::Device> &device) {

    m_voxel_data_buffer = myvk::Buffer::Create(device,
        16 * (1 << 20), 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_octree_info_buffer = myvk::Buffer::Create(device,
        sizeof(OctreeInfo),
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_octree_buffer = myvk::Buffer::Create(device,
        sizeof(uint32_t) * (1 << 20), 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    m_descriptor_set->UpdateUniformBuffer(m_octree_info_buffer, 0);
    m_descriptor_set->UpdateStorageBuffer(m_voxel_data_buffer, 1);
    m_descriptor_set->UpdateStorageBuffer(m_octree_buffer, 2);

    UpdateBuffers();
}

void DynamicOctree::UpdateBuffers() {

    spdlog::info("Updating buffers");

    size_t data_size = m_voxel_data.size() * sizeof(glm::vec4);

    if (data_size > 0) {
        std::shared_ptr<myvk::Buffer> voxel_staging_buffer = myvk::Buffer::CreateStaging(m_device, m_voxel_data.begin(), m_voxel_data.end());
        std::shared_ptr<myvk::Buffer> octree_staging_buffer = myvk::Buffer::CreateStaging(m_device, m_octree.begin(), m_octree.end());

        std::shared_ptr<myvk::Fence> fence = myvk::Fence::Create(m_device);
        std::shared_ptr<myvk::CommandPool> command_pool = myvk::CommandPool::Create(m_graphics_queue);
        std::shared_ptr<myvk::CommandBuffer> command_buffer = myvk::CommandBuffer::Create(command_pool);
        command_buffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        command_buffer->CmdCopy(voxel_staging_buffer, m_voxel_data_buffer, {{0, 0, data_size}});
        command_buffer->CmdCopy(octree_staging_buffer, m_octree_buffer, {{0, 0, sizeof(uint32_t) * (1 << 20)}});
        command_buffer->End();
        command_buffer->Submit(fence);

        fence->Wait();
    }

    m_octree_info_buffer->UpdateData(m_octree_info);
    m_descriptor_set->UpdateUniformBuffer(m_octree_info_buffer, 0);
    m_descriptor_set->UpdateStorageBuffer(m_voxel_data_buffer, 1);
    m_descriptor_set->UpdateStorageBuffer(m_octree_buffer, 2);
}

// Update the octree with completely new geometry
void DynamicOctree::UpdateGeometry(const std::vector<glm::vec4> &voxel_data) {
    m_voxel_data = voxel_data;
    m_octree_info.size = voxel_data.size();

    UpdateBuffers();
}

// Update the octree by adding one voxel
void DynamicOctree::UpdateGeometry(const glm::vec4 &voxel) {
    m_voxel_data.push_back(voxel);
    m_octree_info.size++;
    AddVoxel(voxel);

    UpdateBuffers();
}

void DynamicOctree::DoStep() {
    glm::vec4 vox;
    while ( !m_voxel_generator->DoStep(vox)) {}

    UpdateGeometry(vox);

    if (m_voxel_generator->is_done)
        m_state = false;
}

// Assumes voxel to be in the [1, 2]^3 cube
void DynamicOctree::AddVoxel(const glm::vec4 &vox) {
    glm::vec3 vox_pos = glm::vec3(vox.x, vox.y, vox.z);
    uint32_t header = 1;
    OctreeEntry::ChildDescriptor *parent = &m_octree[m_root].child_descriptor;
    uint32_t resolution = 1;
    glm::vec3 p_min(1.f); // Bottom, front, left corner of current node
    uint32_t child_slot = 0x00;

    // Traverse Octree until vox is inside a leaf node
    while (resolution < m_octree_info.resolution) {

        float scale = 1 / pow(2.f, static_cast<float>(resolution));
        child_slot = 0;

        // Update position
        auto midpoint = p_min + glm::vec3(scale);
        if (vox_pos.x >= midpoint.x) child_slot |= 1u;
        if (vox_pos.y >= midpoint.y) child_slot |= 2u;
        if (vox_pos.z >= midpoint.z) child_slot |= 4u;
        p_min += glm::vec3((child_slot & 1u) >> 0, (child_slot & 2u) >> 1, (child_slot & 4u) >> 2) * scale;

        if (parent->GetValidBit(child_slot) && parent->GetLeafBit(child_slot)) { // A voxel already exists in this node
            return;
        }

        if (!parent->GetValidBit(child_slot)) { // Child slot is empty
            // Add child to parent
            parent->SetValidBit(child_slot);

            // Init new child node
            OctreeEntry child_descriptor{0u};
            uint32_t child_ptr = m_next_free % M_PAGE_SIZE;
            child_descriptor.child_descriptor.SetChildPointer(child_ptr);
            if (m_next_free > header * M_PAGE_SIZE) child_descriptor.child_descriptor.SetFarBit();
            m_octree[parent->GetChildPointer() + child_slot] = child_descriptor;

            // Reserve memory for children
            m_next_free += 8;
            if (m_next_free > M_PAGE_SIZE * header) { // 2 lines for header and far pointer
                m_next_free += 2;
            }
        }

        uint32_t far = parent->GetFarBit() ? M_PAGE_SIZE * header++ : 0;
        parent = &m_octree[far + parent->GetChildPointer() + child_slot].child_descriptor;

        resolution++;
    }

    // max resolution reached
    parent->SetValidBit(child_slot);
    parent->SetLeafBit(child_slot);

    // TODO case: node is completely filled, track parents via stack
}
