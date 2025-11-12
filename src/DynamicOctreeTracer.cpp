//
// Created by Linus on 10.11.2025.
//

#include "DynamicOctreeTracer.hpp"

#include "QuadSpirv.hpp"
#include "myvk/ShaderModule.hpp"
#include "spdlog/spdlog.h"


// public

std::shared_ptr<DynamicOctreeTracer> DynamicOctreeTracer::Create(
    const std::shared_ptr<DynamicOctree> &dynamic_octree,
    const std::shared_ptr<Camera> &camera,
    const std::shared_ptr<Lighting> &lighting,
    const std::shared_ptr<myvk::RenderPass> &render_pass,
    const uint32_t subpass,
    const uint32_t frame_count) {

    std::shared_ptr<DynamicOctreeTracer> ret = std::make_shared<DynamicOctreeTracer>();
    ret->m_dynamic_octree_ptr = dynamic_octree;
    ret->m_camera_ptr = camera;
    ret->m_lighting_ptr = lighting;

    std::shared_ptr<myvk::Device> device = render_pass->GetDevicePtr();
    ret->create_descriptor_pool(device, frame_count);
    ret->create_layouts(device);
    // ret->create_descriptor_sets(frame_count);
    ret->create_main_graphics_pipeline(render_pass, subpass);

    spdlog::info("initialized octree tracer");

    return ret;
}

// private

void DynamicOctreeTracer::create_descriptor_pool(const std::shared_ptr<myvk::Device> &device, const uint32_t frame_count) {
    m_descriptor_pool =
        myvk::DescriptorPool::Create(device, frame_count, {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frame_count}});
}

void DynamicOctreeTracer::create_descriptor_sets(uint32_t frame_count) {
    m_descriptor_sets.resize(frame_count);

    for (auto &descriptor_set : m_descriptor_sets) {
        descriptor_set = myvk::DescriptorSet::Create(m_descriptor_pool, m_descriptor_set_layout);
    }
}

void DynamicOctreeTracer::create_layouts(const std::shared_ptr<myvk::Device> &device) {

    m_main_pipeline_layout = myvk::PipelineLayout::Create(
        device,
        {
            m_dynamic_octree_ptr->GetDescriptorSetLayout(),
            m_camera_ptr->GetDescriptorSetLayout(),
            m_lighting_ptr->GetEnvironmentMapPtr()->GetDescriptorSetLayout()
        },
        {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 2 * sizeof(uint32_t)}}); // TODO constants
}

void DynamicOctreeTracer::create_main_graphics_pipeline(
    const std::shared_ptr<myvk::RenderPass> &render_pass,
    uint32_t subpass) {
    std::shared_ptr<myvk::Device> device = render_pass->GetDevicePtr();

    // create shader modules
    constexpr uint32_t kDynamicOctreeTracerFragSpv[] = {
        #include "spirv/dyn_octree_tracer.frag.u32"
    };
    std::shared_ptr<myvk::ShaderModule> vert_shader_module, frag_shader_module;
    vert_shader_module = myvk::ShaderModule::Create(device, kQuadVertSpv, sizeof(kQuadVertSpv));
    frag_shader_module =
        myvk::ShaderModule::Create(device, kDynamicOctreeTracerFragSpv, sizeof(kDynamicOctreeTracerFragSpv));

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
        vert_shader_module->GetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag_shader_module->GetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    myvk::GraphicsPipelineState pipeline_state = {};
    pipeline_state.m_vertex_input_state.Enable();
    pipeline_state.m_input_assembly_state.Enable(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_state.m_viewport_state.Enable(1, 1);
    pipeline_state.m_rasterization_state.Initialize(VK_POLYGON_MODE_FILL, VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                        VK_CULL_MODE_FRONT_BIT);
    pipeline_state.m_multisample_state.Enable(VK_SAMPLE_COUNT_1_BIT);
    pipeline_state.m_color_blend_state.Enable(1, VK_FALSE);
    pipeline_state.m_dynamic_state.Enable({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

    m_main_graphics_pipeline =
        myvk::GraphicsPipeline::Create(m_main_pipeline_layout, render_pass, shader_stages, pipeline_state, subpass);
}

void DynamicOctreeTracer::CmdDrawPipeline(const std::shared_ptr<myvk::CommandBuffer> &command_buffer,
                                            const uint32_t current_frame) const {
    command_buffer->CmdBindPipeline(m_main_graphics_pipeline);
    command_buffer->CmdBindDescriptorSets(
        {
            m_dynamic_octree_ptr->GetDescriptorSet(),
            m_camera_ptr->GetFrameDescriptorSet(current_frame),
            m_lighting_ptr->GetEnvironmentMapPtr()->GetDescriptorSet()
        },
        m_main_graphics_pipeline);

    VkRect2D scissor = {};
    scissor.extent = {m_width, m_height};
    command_buffer->CmdSetScissor({scissor});
    VkViewport viewport = {};
    viewport.width = m_width;
    viewport.height = m_height;
    command_buffer->CmdSetViewport({viewport});

    uint32_t push_constants[] = {m_width, m_height};
    command_buffer->CmdPushConstants(m_main_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constants), push_constants);

    command_buffer->CmdDraw(3, 1, 0, 0);
}

void DynamicOctreeTracer::Resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
}




