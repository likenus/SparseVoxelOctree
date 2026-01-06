//
// Created by linus on 04.01.2026.
//

#ifndef SPARSEVOXELOCTREE_STACKALLOCATOR_HPP
#define SPARSEVOXELOCTREE_STACKALLOCATOR_HPP
#include "Counter.hpp"
#include "myvk/Buffer.hpp"
#include "myvk/Device.hpp"


class StackAllocator {

private:
    std::shared_ptr<myvk::Device> m_device;
    std::shared_ptr<myvk::CommandPool> m_command_pool;
    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;

    std::shared_ptr<myvk::Buffer> m_stack_buffer;
    Counter m_stack_ptr, m_counter;

    uint32_t m_size = 0;

    void create_buffers(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::CommandPool> &command_pool) {
        m_stack_ptr.Initialize(device);
        m_stack_ptr.Reset(m_command_pool);
        m_counter.Initialize(device);
        m_counter.Reset(m_command_pool, 8);
        m_stack_buffer = myvk::Buffer::Create(device, m_size * sizeof(uint32_t), 0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
    void create_descriptors(const std::shared_ptr<myvk::Device> &device) {
        {
            VkDescriptorSetLayoutBinding stack_binding = {};
            stack_binding.binding = 0;
            stack_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            stack_binding.descriptorCount = 1;
            stack_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            VkDescriptorSetLayoutBinding stack_ptr_binding = {};
            stack_ptr_binding.binding = 1;
            stack_ptr_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            stack_ptr_binding.descriptorCount = 1;
            stack_ptr_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            VkDescriptorSetLayoutBinding counter_binding = {};
            counter_binding.binding = 2;
            counter_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            counter_binding.descriptorCount = 1;
            counter_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            m_descriptor_set_layout = myvk::DescriptorSetLayout::Create(device,
                {stack_binding, stack_ptr_binding, counter_binding});
        }
        m_descriptor_pool = myvk::DescriptorPool::Create(m_device, 1, {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}});
        m_descriptor_set = myvk::DescriptorSet::Create(m_descriptor_pool, m_descriptor_set_layout);

        m_descriptor_set->UpdateStorageBuffer(m_stack_buffer, 0);
        m_descriptor_set->UpdateStorageBuffer(m_stack_ptr.GetBuffer(), 1);
        m_descriptor_set->UpdateStorageBuffer(m_counter.GetBuffer(), 2);
    }
public:
    static std::shared_ptr<StackAllocator> Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::CommandPool> &command_pool, const uint32_t size) {
        auto ret = std::make_shared<StackAllocator>();
        ret->m_device = device;
        ret->m_command_pool = command_pool;
        ret->m_size = size;

        ret->create_buffers(device, command_pool);
        ret->create_descriptors(device);

        return ret;
    }

    std::shared_ptr<myvk::DescriptorSet> GetDescriptorSet() {
        return m_descriptor_set;
    }

    std::shared_ptr<myvk::DescriptorSetLayout> GetDescriptorSetLayout() {
        return m_descriptor_set_layout;
    }

    void Reset() {
        m_stack_ptr.Reset(m_command_pool);
        m_counter.Reset(m_command_pool);
    }

    VkDeviceSize GetRange() const {
        return (m_counter.Read(m_command_pool)) * sizeof(uint32_t);
    }
};


#endif //SPARSEVOXELOCTREE_STACKALLOCATOR_HPP