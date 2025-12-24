//
// Created by linus on 08.11.2025.
//

#include "VoxelGenerator.h"
#include <unordered_map>
#include <chrono>
#include <iostream>

#include "Quaternion.hpp"
#include "Timer.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "myvk/CommandBuffer.hpp"
#include "myvk/Fence.hpp"
#include "spdlog/spdlog.h"

#define COLOR_MASK 0x00ffffffu

static void print_vec(glm::vec3 v) {
    std::cout << v.x << " " << v.y << " " << v.z << std::endl;
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
    auto timer = Timer::start();
    ret->m_sequence = ls.iterate(std::max(1u, depth - 3));
    spdlog::info("Finished iterating string in {} ms.", timer.lap());
    ret->scale = std::pow(2.f, static_cast<float>(depth));
    ret->step = 1 / ret->scale;
    ret->m_depth = depth - 3;
    ret->m_voxel_resolution = 1u << depth;
    ret->delta = 22.5f * (M_PI / 180.f);
    for (char c : axiom) {
        ret->m_long_axiom.push_back(static_cast<uint32_t>(c));
    }
    ret->m_num_terminals = count_terminals(axiom);

    ret->m_device = device;
    //ret->create_buffers(device);
    //ret->create_descriptors(device);
    //ret->create_pipeline(device);


    return ret;
}

void VoxelGenerator::create_buffers(const std::shared_ptr<myvk::Device> &device) {
    uint32_t fragment_count = estimate_fragment_count();

    m_axiom_buffer = myvk::Buffer::Create(device, m_axiom.length() * sizeof(uint32_t), 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_axiom_info_buffer = myvk::Buffer::Create(device, sizeof(AxiomInfo), 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_turtle_constant_buffer = myvk::Buffer::Create(device, sizeof(TurtleConstants), 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_translation_buffer = myvk::Buffer::Create(device, 2 * fragment_count * sizeof(float) * 4,
        0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_rotation_buffer = myvk::Buffer::Create(device, 2 * fragment_count * sizeof(float) * 4, 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_voxel_fragment_buffer = myvk::Buffer::Create(device, m_fragment_list.size() * sizeof(uint32_t) * 2, 0,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_debug_buffer = myvk::Buffer::Create(device, 2 * fragment_count * sizeof(float) * 4,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

void VoxelGenerator::create_descriptors(const std::shared_ptr<myvk::Device> &device) {
    m_descriptor_pool = myvk::DescriptorPool::Create(device, 1,
{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6}});
    {
        VkDescriptorSetLayoutBinding axiom_binding = {};
        axiom_binding.binding = 0;
        axiom_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        axiom_binding.descriptorCount = 1;
        axiom_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding axiom_info_binding = {};
        axiom_info_binding.binding = 1;
        axiom_info_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        axiom_info_binding.descriptorCount = 1;
        axiom_info_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding turtle_constant_binding = {};
        turtle_constant_binding.binding = 2;
        turtle_constant_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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
    m_descriptor_set->UpdateStorageBuffer(m_axiom_info_buffer, 1);
    m_descriptor_set->UpdateStorageBuffer(m_turtle_constant_buffer, 2);
    m_descriptor_set->UpdateStorageBuffer(m_translation_buffer, 3);
    m_descriptor_set->UpdateStorageBuffer(m_rotation_buffer, 4);
    m_descriptor_set->UpdateStorageBuffer(m_voxel_fragment_buffer, 5);
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

    {
        constexpr uint32_t kGeneratorModifyArgCompSpv[] {
#include "spirv/voxel_generator_modify_arg.comp.u32"
        };
        std::shared_ptr<myvk::ShaderModule> generator_modify_arg_shader_module =
            myvk::ShaderModule::Create(device, kGeneratorModifyArgCompSpv, sizeof(kGeneratorModifyArgCompSpv));
        m_generator_modify_arg_pipeline =
            myvk::ComputePipeline::Create(m_pipeline_layout, generator_modify_arg_shader_module);
    }
}

void VoxelGenerator::CmdGenerate(const std::shared_ptr<myvk::CommandBuffer> &command_buffer) {
    Generate();

    m_voxel_fragment_staging_buffer = myvk::Buffer::CreateStaging(m_device, m_fragment_list.begin(), m_fragment_list.end());
    command_buffer->CmdCopy(m_voxel_fragment_staging_buffer, m_voxel_fragment_buffer, {{0, 0, m_voxel_fragment_staging_buffer->GetSize()}});
    command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
         {m_voxel_fragment_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
         {});
}

// void VoxelGenerator::CmdGenerate(std::shared_ptr<myvk::CommandBuffer> &command_buffer) {
//
//
//     TurtleConstants turtle_constants{{0.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {.5f, 0.f,.5f}, delta, step};
//     uint32_t num_terminals = m_num_terminals;
//     AxiomInfo axiom_info{static_cast<uint32_t>(m_axiom.length()), 1, 1, m_depth + 3, estimate_fragment_count(), num_terminals};
//     m_turtle_constants_staging_buffer = myvk::Buffer::CreateStaging<TurtleConstants>(m_device, turtle_constants);
//     m_axiom_staging_buffer = myvk::Buffer::CreateStaging(m_device, m_long_axiom.begin(), m_long_axiom.end());
//     m_axiom_info_staging_buffer = myvk::Buffer::CreateStaging<AxiomInfo>(m_device, axiom_info);
//
//     command_buffer->CmdCopy(m_axiom_staging_buffer, m_axiom_buffer, {{0, 0, m_axiom_buffer->GetSize()}});
//     command_buffer->CmdCopy(m_turtle_constants_staging_buffer, m_turtle_constant_buffer, {{0, 0, m_turtle_constants_staging_buffer->GetSize()}});
//     command_buffer->CmdCopy(m_axiom_info_staging_buffer, m_axiom_info_buffer, {{0, 0, m_axiom_info_buffer->GetSize()}});
//
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
// {m_axiom_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
//                         m_turtle_constant_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
//                         m_axiom_info_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
// {});
//
//     m_rotation_staging_buffer = myvk::Buffer::CreateStaging<glm::vec4>(m_device, num_terminals,
//         [num_terminals](glm::vec4 *data) {
//             for (int i = 0; i < num_terminals; i++) {
//                 data[i] = glm::vec4(1.f, 0.f, 0.f, 0.f); // identity quaternion
//             }
//         }
//     );
//     m_translation_staging_buffer = myvk::Buffer::CreateStaging<glm::vec4>(m_device, num_terminals,
//         [num_terminals, &turtle_constants](glm::vec4 *data) {
//             for (int i = 0; i < num_terminals; i++) {
//                 data[i] = glm::vec4(turtle_constants._O, 0.f); // no translation
//             }
//         }
//     );
//
//     command_buffer->CmdCopy(m_rotation_staging_buffer, m_rotation_buffer, {{0, 0, m_rotation_staging_buffer->GetSize()}});
//     command_buffer->CmdCopy(m_translation_staging_buffer, m_translation_buffer, {{0, 0, m_translation_staging_buffer->GetSize()}});
//
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
//         {m_rotation_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
//                                 m_translation_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
//         {});
//
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
//         {m_turtle_constant_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
//                                 m_axiom_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)},
//         {});
//
//     command_buffer->CmdBindDescriptorSets({m_descriptor_set}, m_pipeline_layout, VK_PIPELINE_BIND_POINT_COMPUTE, {});
//
//     uint32_t axiom_length = 1;
//     for (uint32_t depth = 0; depth < m_depth; depth++) {
//
//         // Iterate string
//         command_buffer->CmdBindPipeline(m_generator_iterate_pipeline);
//         command_buffer->CmdDispatch((axiom_length + 63) / 64, 1, 1);
//
//         command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
//             {m_translation_buffer->GetMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
//                                     m_rotation_buffer->GetMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
//             {});
//
//         // Increment offsets
//         command_buffer->CmdBindPipeline(m_generator_modify_arg_pipeline);
//         command_buffer->CmdDispatch(1, 1, 1);
//
//         command_buffer->CmdPipelineBarrier(
//             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
//                 {m_axiom_info_buffer->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)},
//                 {});
//
//         axiom_length *= num_terminals;
//     }
//
//     // Generate Fragments
//     command_buffer->CmdBindPipeline(m_generator_generate_pipeline);
//     command_buffer->CmdDispatch((estimate_fragment_count() + 63) / 64, 1, 1);
//
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
//         {m_voxel_fragment_buffer->GetMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
//         {});
//
//     // Debug dump
//     auto buffer_to_debug = m_translation_buffer;
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {},
//         {buffer_to_debug->GetMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
//             VK_ACCESS_TRANSFER_READ_BIT)},
//         {});
//
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {},
//         {buffer_to_debug->GetMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)},
//         {});
//
//     command_buffer->CmdCopy(buffer_to_debug, m_debug_buffer, {{0, 0, buffer_to_debug->GetSize()}});
//
//     command_buffer->CmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, {},
//         {buffer_to_debug->GetMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)},
//         {});
// }


uint32_t VoxelGenerator::estimate_fragment_count() const {
    return std::pow(m_num_terminals, m_depth);
}

bool VoxelGenerator::DoStep(glm::vec4 &out_pos) {
    if (m_pos >= m_sequence.length()) {
        is_done = true;
        return false; // Potentially grow
    }

    while (m_pos < m_sequence.length()) {

        switch (m_sequence[m_pos++]) {
            case 'F': {
                t.p += t.d * step;
                //glm::vec4 pos = glm::vec4(std::round(t.p.x * scale) / scale, std::round(t.p.y * scale) / scale, std::round(t.p.z * scale) / scale, 0.f) + (step / 2.f);
                glm::vec4 pos = glm::vec4(t.p, 0.f);
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
            case '+': t.d = Quaternion::rotate(t.d, -delta, t.n); break;
            case '-': t.d = Quaternion::rotate(t.d, delta, t.n); break;
            case '<': t.n = Quaternion::rotate(t.n, -delta, t.d); break;
            case '>': t.n = Quaternion::rotate(t.n, delta, t.d); break;

            case '[':
                stack.push_back(t); break;
            case ']':
                t = stack.back();
                stack.pop_back();
                break;
            default:
                break;
        }
    }

    return false;
}

void VoxelGenerator::Generate() {
    spdlog::info("Begin generating geometry");
    auto timer = Timer::start();
    glm::vec4 _;

    while (!is_done) { DoStep(_); } // Run until all voxels are generated

    spdlog::info("Finished generating geometry in {} ms. {} Voxels generated",
        timer.lap(),
        m_fragment_list.size());
    m_voxel_fragment_buffer = myvk::Buffer::Create(m_device, m_fragment_list.size() * sizeof(uint32_t) * 2, 0,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

void VoxelGenerator::DumpBuffer() {
    glm::vec4 *buffer_data = static_cast<glm::vec4 *>(m_debug_buffer->GetMappedData());



    std::cout << "Finished dumping buffer" << std::endl;

}

uint32_t VoxelGenerator::count_terminals(std::string axiom) {
    uint32_t count = 0;

    for (char c: axiom) {
        count += (c == 'F');
    }

    return count;
}


