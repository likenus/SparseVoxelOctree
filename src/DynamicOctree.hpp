//
// Created by Linus on 10.11.2025.
//

#ifndef SPARSEVOXELOCTREE_DYNAMICOCTREE_HPP
#define SPARSEVOXELOCTREE_DYNAMICOCTREE_HPP
#include <memory>

#include "myvk/Buffer.hpp"
#include "myvk/DescriptorPool.hpp"
#include "myvk/DescriptorSet.hpp"
#include "myvk/DescriptorSetLayout.hpp"


class DynamicOctree {
private:
    std::shared_ptr<myvk::Buffer> m_voxel_buffer;
    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;

    void CreateBuffers(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::Queue> &graphics_queue);

public:
    static std::shared_ptr<DynamicOctree> Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::Queue> &graphics_queue);

    const std::shared_ptr<myvk::Buffer> &GetBuffer() const { return m_voxel_buffer; }
    const std::shared_ptr<myvk::DescriptorSetLayout> &GetDescriptorSetLayout() const { return m_descriptor_set_layout; }
    const std::shared_ptr<myvk::DescriptorSet> &GetDescriptorSet() const { return m_descriptor_set; }
};


#endif