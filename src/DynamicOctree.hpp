//
// Created by Linus on 10.11.2025.
//

#ifndef SPARSEVOXELOCTREE_DYNAMICOCTREE_HPP
#define SPARSEVOXELOCTREE_DYNAMICOCTREE_HPP

#define FULL_MASK 0xff00
#define FULL_LEAF 0x00ff
#define M_PAGE_SIZE 8192
#define FAR_BIT 0x10000
#define CHILD_PTR_MASK 0xfffe0000

#include <memory>

#include "VoxelGenerator.h"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "myvk/Buffer.hpp"
#include "myvk/DescriptorPool.hpp"
#include "myvk/DescriptorSet.hpp"
#include "myvk/DescriptorSetLayout.hpp"


class DynamicOctree {
private:
    std::shared_ptr<myvk::Device> m_device;
    std::shared_ptr<myvk::Queue> m_graphics_queue;

    std::shared_ptr<myvk::Buffer> m_voxel_data_buffer;
    std::shared_ptr<myvk::Buffer> m_octree_info_buffer;
    std::shared_ptr<myvk::Buffer> m_octree_buffer;
    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::shared_ptr<myvk::DescriptorSet> m_descriptor_set;
    std::shared_ptr<VoxelGenerator> m_voxel_generator;

    struct OctreeInfo {
        uint32_t size = 0;
        uint32_t resolution = 8; // Dictates the smallest voxel, the octree can store
    };
    OctreeInfo m_octree_info;
    std::vector<glm::vec4> m_voxel_data;

    union OctreeEntry {
        struct ChildDescriptor {
            uint32_t descriptor;

            uint32_t GetChildPointer() const {
                return descriptor >> 17;
            }

            void SetChildPointer(const uint32_t ptr) {
                descriptor &= CHILD_PTR_MASK;
                descriptor |= ptr << 17;
            }

            bool GetFarBit() const {
                return descriptor & FAR_BIT;
            }

            bool GetValidBit(uint32_t child_idx) {
                return descriptor & (0x10 << child_idx);
            }

            uint32_t GetValidMask() const {
                return descriptor & 0xff00;
            }

            bool GetLeafBit(const uint32_t child_idx) const {
                return descriptor & (0x1 << child_idx);
            }

            uint32_t GetLeafMask() const {
                return descriptor & 0xff;
            }

            void SetValidBit(uint32_t child_idx) {
                descriptor |= (0x10 << child_idx);
            }

            void SetLeafBit(uint32_t child_idx) {
                descriptor |= (0x1 << child_idx);
            }

            void SetFarBit() {
                descriptor |= FAR_BIT;
            }
        } child_descriptor;
        uint32_t page_header;
        uint32_t far_pointer;
    };

    std::vector<OctreeEntry> m_octree;
    uint32_t m_root = 1;
    uint32_t m_next_free = 1 + 1 + 8; // page header + root + root children

    void CreateBuffers(const std::shared_ptr<myvk::Device> &device);
    void UpdateBuffers();


public:
    bool m_state = false;

    static std::shared_ptr<DynamicOctree> Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::Queue> &graphics_queue);

    const std::shared_ptr<myvk::Buffer> &GetBuffer() const { return m_voxel_data_buffer; }
    const std::shared_ptr<myvk::DescriptorSetLayout> &GetDescriptorSetLayout() const { return m_descriptor_set_layout; }
    const std::shared_ptr<myvk::DescriptorSet> &GetDescriptorSet() const { return m_descriptor_set; }

    void UpdateGeometry(const std::vector<glm::vec4> &voxel_data);
    void UpdateGeometry(const glm::vec4 &voxel);
    void DoStep();
    bool IsRunning() const { return m_state; }

    void AddVoxel(const glm::vec4 &vox);
    void Build(const std::vector<glm::vec4> &voxels);
};



#endif