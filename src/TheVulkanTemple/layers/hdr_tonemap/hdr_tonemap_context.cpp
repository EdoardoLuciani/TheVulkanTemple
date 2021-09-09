#include "hdr_tonemap_context.h"
#include <vector>
#include <array>
#include <utility>
#include <unordered_map>
#include "../../vulkan_helper.h"
#include "../../external/volk.h"

HDRTonemapContext::HDRTonemapContext(VkDevice device, VkFormat input_image_format, VkFormat global_ao_image_format, VkFormat out_format) {
    this->device = device;

    std::array<VkAttachmentDescription, 3> attachment_descriptions {{
    {
            0,
            input_image_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    },
    {
            0,
            global_ao_image_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    },
    {
            0,
            out_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    }
    }};
    std::array<VkAttachmentReference,3> attachment_references {{
        { 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    }};
    VkSubpassDescription subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            2,
            &attachment_references[0],
            1,
            &attachment_references[2],
            nullptr,
            nullptr,
            0,
            nullptr
    };
    VkRenderPassCreateInfo render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            attachment_descriptions.size(),
            attachment_descriptions.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &hdr_tonemap_render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // We create the descriptor set layout for the compute shader
    std::array<VkDescriptorSetLayoutBinding, 2> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hdr_tonemap_set_layout), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);
}

HDRTonemapContext::~HDRTonemapContext() {
    vkDeviceWaitIdle(device);

    for(auto& framebuffer : hdr_tonemap_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyRenderPass(device, hdr_tonemap_render_pass, nullptr);
    vkDestroyPipelineLayout(device, hdr_tonemap_pipeline_layout, nullptr);
    vkDestroyPipeline(device, hdr_tonemap_pipeline, nullptr);

    vkDestroyDescriptorSetLayout(device, hdr_tonemap_set_layout, nullptr);
}

void HDRTonemapContext::create_resources(VkExtent2D screen_res, std::string shader_dir_path) {
    // We get the output images count in the resources method
    this->out_image_res = screen_res;

    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//tonemap.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//tonemap.frag.spv", shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info {{
    {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_shader_module,
            "main",
            nullptr
    },
    {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader_module,
            "main",
            nullptr
    }
    }};

    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            0,
            nullptr,
            0,
            nullptr
    };

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_FALSE
    };

    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(screen_res.width),
            static_cast<float>(screen_res.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {screen_res.width, screen_res.height}
    };
    VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            0,
            1,
            &viewport,
            1,
            &scissor
    };

    VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f
    };

    VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            1.0f,
            nullptr,
            VK_FALSE,
            VK_FALSE
    };

    VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_COMPARE_OP_NEVER,
            VK_FALSE,
            VK_FALSE,
            {},
            {},
            0.0f,
            1.0f
    };

    std::array<VkPipelineColorBlendAttachmentState, 1> pipeline_color_blend_attachment_state;
    pipeline_color_blend_attachment_state[0] = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            pipeline_color_blend_attachment_state.size(),
            pipeline_color_blend_attachment_state.data(),
            {0.0f,0.0f,0.0f,0.0f}
    };

    std::array<VkDynamicState,2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamic_states.size(),
            dynamic_states.data()
    };

    std::array<VkDescriptorSetLayout,1> descriptor_set_layouts = {hdr_tonemap_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layouts.size(),
            descriptor_set_layouts.data(),
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, hdr_tonemap_pipeline_layout, nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &hdr_tonemap_pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &pipeline_vertex_input_state_create_info,
            &pipeline_input_assembly_create_info,
            nullptr,
            &pipeline_viewport_state_create_info,
            &pipeline_rasterization_state_create_info,
            &pipeline_multisample_state_create_info,
            &pipeline_depth_stencil_state_create_info,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            hdr_tonemap_pipeline_layout,
            hdr_tonemap_render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, hdr_tonemap_pipeline, nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &hdr_tonemap_pipeline);

    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> HDRTonemapContext::get_required_descriptor_pool_size_and_sets() {
    return {
            { {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2}},
            1};
}

void HDRTonemapContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView input_ao_image_view, std::vector<VkImageView> out_image_views) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            1,
            &hdr_tonemap_set_layout
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, &hdr_tonemap_descriptor_set), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    std::array<VkDescriptorImageInfo, 2> descriptor_image_infos;
    descriptor_image_infos[0] = {
        VK_NULL_HANDLE,
        input_image_view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    descriptor_image_infos[1] = {
        VK_NULL_HANDLE,
        input_ao_image_view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    std::array<VkWriteDescriptorSet, 2> write_descriptor_set;
    write_descriptor_set[0] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        hdr_tonemap_descriptor_set,
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        &descriptor_image_infos[0],
        nullptr,
        nullptr
    };
    write_descriptor_set[1] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        hdr_tonemap_descriptor_set,
        1,
        0,
        1,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        &descriptor_image_infos[1],
        nullptr,
        nullptr
    };
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);

    // Creation of the framebuffer
    // Framebuffer creation
    for(auto& framebuffer : hdr_tonemap_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    hdr_tonemap_framebuffers.clear();
    hdr_tonemap_framebuffers.resize(out_image_views.size());

    for (size_t i = 0; i < hdr_tonemap_framebuffers.size(); i++) {
        std::array<VkImageView, 3> attachments = {
                input_image_view,
                input_ao_image_view,
                out_image_views[i]
        };
        VkFramebufferCreateInfo framebuffer_create_info = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                hdr_tonemap_render_pass,
                attachments.size(),
                attachments.data(),
                out_image_res.width,
                out_image_res.height,
                1
        };
        vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hdr_tonemap_framebuffers[i]);
    }
}

void HDRTonemapContext::record_into_command_buffer(VkCommandBuffer command_buffer, uint32_t out_image_index, VkExtent2D image_size) {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdr_tonemap_pipeline);
    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(image_size.width),
            static_cast<float>(image_size.height),
            0.0f,
            1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
            {0,0},
            {image_size.width, image_size.height}
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    std::array<VkClearValue, 3> clear_values;
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clear_values[1].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clear_values[2].color = {0.0f, 0.0f, 0.0f, 0.0f};
    VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            hdr_tonemap_render_pass,
            hdr_tonemap_framebuffers[out_image_index],
            {{0,0},{image_size.width, image_size.height}},
            clear_values.size(),
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdr_tonemap_pipeline_layout, 0, 1, &hdr_tonemap_descriptor_set, 0, nullptr);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}
