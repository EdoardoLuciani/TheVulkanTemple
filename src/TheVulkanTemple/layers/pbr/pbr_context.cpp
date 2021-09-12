#include "pbr_context.h"

PbrContext::PbrContext(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties, VkFormat out_depth_image_format,
                       VkFormat out_color_image_format, VkFormat out_normal_image_format) {
    this->device = device;
    this->physical_device_memory_properties = memory_properties;
    // Creating the renderpass with 3 outputs: depth, color and normal
    std::array<VkAttachmentDescription, 3> attachment_descriptions {{{
        0,
        out_depth_image_format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },{
        0,
        out_color_image_format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },{
        0,
        out_normal_image_format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }
    }};
    std::array<VkAttachmentReference,3> attachment_references {{
        { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    }};
    VkSubpassDescription subpass_description = {
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        nullptr,
        2,
        &attachment_references[1],
        nullptr,
        &attachment_references[0],
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
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &pbr_render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);
}

PbrContext::~PbrContext() {
    vkDestroyRenderPass(device, pbr_render_pass, nullptr);
    vkDestroyPipelineLayout(device, pbr_pipeline_layout, nullptr);
    vkDestroyPipeline(device, pbr_pipeline, nullptr);
    vkDestroyFramebuffer(device, pbr_framebuffer, nullptr);
}

void PbrContext::create_pipeline(std::string shader_dir_path, VkDescriptorSetLayout pbr_model_data_set_layout,
                                     VkDescriptorSetLayout camera_data_set_layout, VkDescriptorSetLayout light_data_set_layout) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//pbr.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//pbr.frag.spv", shader_contents);
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

    VkVertexInputBindingDescription vertex_input_binding_description = {
            0,
            12 * sizeof(float),
            VK_VERTEX_INPUT_RATE_VERTEX
    };
    std::array<VkVertexInputAttributeDescription,4> vertex_input_attribute_description  {{
        {
            0,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            0
            },
            {
            1,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            3 * sizeof(float)
            },
            {
            2,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            5 * sizeof(float)
            },
            {
            3,
            0,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            8 * sizeof(float)
            }
    }};
    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            1,
            &vertex_input_binding_description,
            vertex_input_attribute_description.size(),
            vertex_input_attribute_description.data()
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
            VK_CULL_MODE_BACK_BIT,
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
            VK_TRUE,
            VK_TRUE,
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            {},
            {},
            0.0f,
            1.0f
    };

    std::array<VkPipelineColorBlendAttachmentState, 2> pipeline_color_blend_attachment_state;
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
    pipeline_color_blend_attachment_state[1] = {
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

    std::array<VkDescriptorSetLayout,3> descriptor_set_layouts = {pbr_model_data_set_layout, light_data_set_layout, camera_data_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layouts.size(),
            descriptor_set_layouts.data(),
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, pbr_pipeline_layout, nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pbr_pipeline_layout);

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
            pbr_pipeline_layout,
            pbr_render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, pbr_pipeline, nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pbr_pipeline);

    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void PbrContext::set_output_images(VkExtent2D screen_res, VkImageView out_depth_image, VkImageView out_color_image, VkImageView out_normal_image) {
    this->screen_res = screen_res;
    std::array<VkImageView, 3> attachments {out_depth_image, out_color_image, out_normal_image};
    VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            pbr_render_pass,
            attachments.size(),
            attachments.data(),
            screen_res.width,
            screen_res.height,
            1
    };
    vkDestroyFramebuffer(device, pbr_framebuffer, nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &pbr_framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
}

void PbrContext::record_into_command_buffer(VkCommandBuffer command_buffer, VkDescriptorSet camera_descriptor_set, VkDescriptorSet light_descriptor_set,
		const std::vector<VkModel> &vk_models, const Camera &camera) {
    std::array<VkClearValue,3> clear_values;
    clear_values[0].depthStencil = {1.0f, 0};
    clear_values[1].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clear_values[2].color = {0.0f, 0.0f, 0.0f, 0.0f};

    VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            pbr_render_pass,
            pbr_framebuffer,
            {{0,0},{this->screen_res.width, this->screen_res.height}},
            clear_values.size(),
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline);
    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(this->screen_res.width),
            static_cast<float>(this->screen_res.height),
            0.0f,
            1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
            {0,0},
            {screen_res.width, screen_res.height}
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    std::vector<VkDescriptorSet> to_bind = { light_descriptor_set, camera_descriptor_set };
    for (uint32_t j=0; j<vk_models.size(); j++) {
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline_layout, 1, to_bind.size(), to_bind.data(), 0, nullptr);
        vk_models[j].vk_record_draw(command_buffer, pbr_pipeline_layout, 0, &camera);
    }
    vkCmdEndRenderPass(command_buffer);
}
