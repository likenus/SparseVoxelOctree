//
// Created by linus on 08.11.2025.
//

#include "VoxelGenerator.h"
#include <unordered_map>
#include <chrono>

#include "glm/gtx/rotate_vector.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/Fence.hpp"
#include "spdlog/spdlog.h"

#define COLOR_MASK 0x00ffffffu

void VoxelGenerator::create_buffers(const std::shared_ptr<myvk::Device> &device) {
    uint32_t fragment_count = estimate_fragment_count();

    m_axiom_buffer = myvk::Buffer::Create(device, m_axiom.length() * sizeof(uint32_t), 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_axiom_info_buffer = myvk::Buffer::Create(device, 0, 0,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_turtle_constant_buffer = myvk::Buffer::Create(device, sizeof(TurtleConstants), 0,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_translation_buffer = myvk::Buffer::Create(device, fragment_count * sizeof(float) * 4, 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_rotation_buffer = myvk::Buffer::Create(device, fragment_count * sizeof(float) * 4, 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_voxel_fragment_buffer = myvk::Buffer::Create(device, fragment_count * sizeof(uint32_t) * 2, 0,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

void VoxelGenerator::create_descriptors(const std::shared_ptr<myvk::Device> &device) {
    m_descriptor_pool = myvk::DescriptorPool::Create(device, 1,
{
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
        });
    {
        VkDescriptorSetLayoutBinding axiom_binding = {};
        axiom_binding.binding = 0;
        axiom_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        axiom_binding.descriptorCount = 1;
        axiom_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding axiom_info_binding = {};
        axiom_info_binding.binding = 1;
        axiom_info_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        axiom_info_binding.descriptorCount = 1;
        axiom_info_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding turtle_constant_binding = {};
        turtle_constant_binding.binding = 2;
        turtle_constant_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        turtle_constant_binding.descriptorCount = 1;
        turtle_constant_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding translation_binding = {};
        translation_binding.binding = 3;
        translation_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        translation_binding.descriptorCount = 1;
        translation_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding rotation_binding = {};
        rotation_binding.binding = 4;
        rotation_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        rotation_binding.descriptorCount = 1;
        rotation_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding fragment_list_binding = {};
        fragment_list_binding.binding = 5;
        fragment_list_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        fragment_list_binding.descriptorCount = 1;
        fragment_list_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        m_descriptor_set_layout =
            myvk::DescriptorSetLayout::Create(device,
                {axiom_binding, axiom_info_binding, turtle_constant_binding,
                    translation_binding, rotation_binding, fragment_list_binding});
    }
    m_descriptor_set = myvk::DescriptorSet::Create(m_descriptor_pool, m_descriptor_set_layout);
    m_descriptor_set->UpdateStorageBuffer(m_axiom_buffer, 0);
    m_descriptor_set->UpdateUniformBuffer(m_turtle_constant_buffer, 1);
    m_descriptor_set->UpdateStorageBuffer(m_translation_buffer, 2);
    m_descriptor_set->UpdateStorageBuffer(m_rotation_buffer, 3);
    m_descriptor_set->UpdateStorageBuffer(m_voxel_fragment_buffer, 4);
}

void VoxelGenerator::create_pipeline(const std::shared_ptr<myvk::Device> &device) {
    m_pipeline_layout = myvk::PipelineLayout::Create(device, {m_descriptor_set_layout}, {});

    {
        constexpr uint32_t kGeneratorIterateCompSpv[] = {
            #include "spirv/voxel_generator_iterate.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> generator_iterate_shader_module =
            myvk::ShaderModule::Create(device, kGeneratorIterateCompSpv, sizeof(kGeneratorIterateCompSpv));
        m_generator_iterate_pipeline =
            myvk::ComputePipeline::Create(m_pipeline_layout, generator_iterate_shader_module);
    }

    {
        constexpr uint32_t kGeneratorGenerateCompSpv[] {
#include "spirv/voxel_generator_generate.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> generator_generate_shader_module =
            myvk::ShaderModule::Create(device, kGeneratorGenerateCompSpv, sizeof(kGeneratorGenerateCompSpv));
        m_generator_generate_pipeline =
            myvk::ComputePipeline::Create(m_pipeline_layout, generator_generate_shader_module);
    }
}

uint32_t VoxelGenerator::estimate_fragment_count() const {
    return std::pow(7u, depth - 3) * 2;
}


class LSystem {
public:
    std::string axiom;
    std::unordered_map<char, std::string> rules;

    LSystem(const std::string& ax) : axiom(ax) {}

    void addRule(char symbol, const std::string& expansion) {
        rules[symbol] = expansion;
    }

    std::string iterate(const uint32_t depth) {
        std::string result = axiom;

        for (int i = 0; i < depth; i++) {
            std::string next;
            for (char c : result) {
                if (rules.contains(c))
                    next += rules[c];
                else
                    next += c;
            }
            result = next;
        }
        return result;
    }
};

std::shared_ptr<VoxelGenerator> VoxelGenerator::Create(const std::shared_ptr<myvk::Device> &device, const std::shared_ptr<myvk::CommandPool> &command_pool,
    const std::string &axiom, const uint32_t depth) {
    auto ret = std::make_shared<VoxelGenerator>();

    LSystem ls("F");
    ls.addRule('F', axiom);
    ret->m_axiom = axiom;
    ret->m_sequence = ls.iterate(std::max(1u, depth - 3));
    ret->scale = std::pow(2.f, static_cast<float>(depth));
    ret->step = 1 / ret->scale;
    ret->depth = depth;
    ret->m_voxel_resolution = 1u << depth;

    ret->create_buffers(device);
    ret->create_descriptors(device);
    ret->create_pipeline(device);

    ret->Generate(command_pool);

    return ret;
}

bool VoxelGenerator::DoStep(glm::vec4 &out_pos) {
    if (m_pos >= m_sequence.length()) {
        is_done = true;
        return false; // Potentially grow
    }

    switch (m_sequence[m_pos++]) {
        case 'F': {
            t.p += t.d * step;

            glm::vec4 pos = glm::vec4(std::round(t.p.x * scale) / scale, std::round(t.p.y * scale) / scale, std::round(t.p.z * scale) / scale, 0.f) + (step / 2.f);
            out_pos = pos;
            glm::uvec2 fragment;
            uint32_t x, y, z;
            x = static_cast<uint32_t>(pos.x * m_voxel_resolution);
            y = static_cast<uint32_t>(pos.y * m_voxel_resolution);
            z = static_cast<uint32_t>(pos.z * m_voxel_resolution);

            uint32_t color = 0x00092a3b;
            if (m_pos == m_sequence.size() || m_sequence[m_pos] == ']') {
                color = 0x00043d0e;
            }

            fragment.x = x | (y << 12) | ((z & 0xff) << 24);
            fragment.y = ((z >> 8) << 28) | COLOR_MASK & color;
            m_fragment_list.push_back(fragment);
            return true;
        }

        case '+': t.d = glm::rotate(t.d, delta, t.n); break;
        case '-': t.d = glm::rotate(t.d, -delta, t.n); break;
        case '<': t.n = glm::rotate(t.n, -delta, t.d); break;
        case '>': t.n = glm::rotate(t.n, delta, t.d); break;

        case '[':
            stack.push_back(t); break;
        case ']':
            t = stack.back();
            stack.pop_back();
            break;
        default:
            break;
    }

    return false;
}

void VoxelGenerator::Generate(const std::shared_ptr<myvk::CommandPool> &command_pool) {
    spdlog::info("Begin generating geometry");

    auto start = std::chrono::high_resolution_clock::now();
    glm::vec4 _;
    while (!is_done) { DoStep(_); } // Run until all voxels are generated

    spdlog::info("Finished generating geometry. {} Voxels generated", m_fragment_list.size());

    std::shared_ptr<myvk::Device> device = command_pool->GetDevicePtr();



    std::shared_ptr<myvk::Buffer> voxel_fragment_staging_buffer = myvk::Buffer::CreateStaging(device, m_fragment_list.begin(), m_fragment_list.end());
    std::shared_ptr<myvk::Fence> fence = myvk::Fence::Create(device);
    std::shared_ptr<myvk::CommandBuffer> command_buffer = myvk::CommandBuffer::Create(command_pool);
    command_buffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    command_buffer->CmdCopy(voxel_fragment_staging_buffer, m_voxel_fragment_buffer, {{0, 0, m_voxel_fragment_buffer->GetSize()}});
    command_buffer->End();
    command_buffer->Submit(fence);

    fence->Wait();

    auto end = std::chrono::high_resolution_clock::now();

    spdlog::info("Finished generating fragment list. Took {} ms.",
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

