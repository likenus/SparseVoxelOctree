//
// Created by Linus on 10.11.2025.
//

#ifndef SPARSEVOXELOCTREE_DYNAMICOCTREETRACER_HPP
#define SPARSEVOXELOCTREE_DYNAMICOCTREETRACER_HPP

#include "Camera.hpp"
#include "DynamicOctree.hpp"
#include "Lighting.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/DescriptorSet.hpp"
#include "myvk/GraphicsPipeline.hpp"

class DynamicOctreeTracer {
private:
    uint32_t m_width{kDefaultWidth}, m_height{kDefaultHeight};

    std::shared_ptr<DynamicOctree> m_dynamic_octree_ptr;
    std::shared_ptr<Camera> m_camera_ptr;
    std::shared_ptr<Lighting> m_lighting_ptr;

    std::shared_ptr<myvk::DescriptorPool> m_descriptor_pool;
    std::shared_ptr<myvk::DescriptorSetLayout> m_descriptor_set_layout;
    std::vector<std::shared_ptr<myvk::DescriptorSet>> m_descriptor_sets;

    std::shared_ptr<myvk::PipelineLayout> m_main_pipeline_layout;

    std::shared_ptr<myvk::GraphicsPipeline> m_main_graphics_pipeline;

    void create_descriptor_pool(const std::shared_ptr<myvk::Device> &device, uint32_t frame_count);
    void create_descriptor_sets(uint32_t frame_count);

    void create_layouts(const std::shared_ptr<myvk::Device> &device);

    void create_main_graphics_pipeline(const std::shared_ptr<myvk::RenderPass> &render_pass, uint32_t subpass);
public:
    static std::shared_ptr<DynamicOctreeTracer> Create(
        const std::shared_ptr<DynamicOctree> &dynamic_octree,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Lighting> &lighting,
        const std::shared_ptr<myvk::RenderPass> &render_pass,
        uint32_t subpass,
        uint32_t frame_count
        );

    const std::shared_ptr<Camera> &GetCameraPtr() const { return m_camera_ptr; }
    const std::shared_ptr<Lighting> &GetLightingPtr() const { return m_lighting_ptr; }
    const std::shared_ptr<DynamicOctree> &GetOctreePtr() const { return m_dynamic_octree_ptr; }

    void Resize(uint32_t width, uint32_t height);

    void CmdDrawPipeline(const std::shared_ptr<myvk::CommandBuffer> &command_buffer, uint32_t current_frame) const;
};


#endif