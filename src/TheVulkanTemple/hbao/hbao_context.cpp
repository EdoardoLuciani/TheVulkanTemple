#include "hbao_context.h"

HbaoContext::HbaoContext(VkDevice device, VkExtent2D screen_res, VkFormat depth_image_format, std::string shader_dir_path) {
    this->device = device;

    VkSamplerCreateInfo sampler_create_info = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_NEAREST,
            VK_FILTER_NEAREST,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            0.0f,
            VK_FALSE,
            0.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0f,
            1.0f,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &do_nothing_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    // Render pass for depth_linearize
    std::array<VkAttachmentDescription, 8> attachment_descriptions;

    // Input attachment for the depth
    attachment_descriptions[0] = {
            0,
            depth_image_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    // Output linearized depth image is of the format VK_FORMAT_R16_SFLOAT
    attachment_descriptions[1] = {
            0,
            VK_FORMAT_R16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    std::array<VkAttachmentReference,8> attachment_references;
    attachment_references[0] = { 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    attachment_references[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            1,
            &attachment_references[0],
            1,
            &attachment_references[1],
            nullptr,
            nullptr,
            0,
            nullptr
    };
    VkRenderPassCreateInfo render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            2,
            attachment_descriptions.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &hbao_render_passes[0]), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // Render pass for deinterleave
    attachment_descriptions.fill({
        0,
        VK_FORMAT_R16_SFLOAT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    for(uint32_t i = 0; i < attachment_references.size(); i++) {
        attachment_references[i] = {i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            8,
            &attachment_references[0],
            nullptr,
            nullptr,
            0,
            nullptr
    };
    render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            8,
            &attachment_descriptions[0],
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &hbao_render_passes[1]), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // We create the descriptor set layout for the depth linearize
    std::array<VkDescriptorSetLayoutBinding, 1> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
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
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hbao_descriptor_set_layouts[0]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // We create the descriptor set layout for the deinterleave
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hbao_descriptor_set_layouts[1]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Then we start creating the pipelines
    // Creating the vertex shader that is common for all the pipelines
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//fullscreen_tri.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    // Creating the pipelines
    create_linearize_depth_pipeline(vertex_shader_module, screen_res, shader_dir_path);
    create_deinterleave_pipeline(vertex_shader_module, screen_res, shader_dir_path);

    // We don't need the vertex shader module anymore
    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
}

void HbaoContext::create_linearize_depth_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//depth_linearize.frag.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

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
            static_cast<float>(render_target_extent.width),
            static_cast<float>(render_target_extent.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {render_target_extent.width, render_target_extent.height}
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
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0xff, 0xff, 1},
            {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0xff, 0xff, 1},
            0.0f,
            1.0f
    };

    VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
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
            1,
            &pipeline_color_blend_attachment_state,
            {0.0f,0.0f,0.0f,0.0f}
    };

    // We set the viewport and scissor dynamically
    std::array<VkDynamicState,2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamic_states.size(),
            dynamic_states.data()
    };

    // We need to tell the pipeline we will use push constants to pass the clip_info
    VkPushConstantRange push_constant_range = {
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(glm::vec4)
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &hbao_descriptor_set_layouts[0],
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &hbao_pipelines_layouts[0]);

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
            hbao_pipelines_layouts[0],
            hbao_render_passes[0],
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &hbao_pipelines[0]);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_deinterleave_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//hbao_deinterleave.frag.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

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
            static_cast<float>(render_target_extent.width / 4),
            static_cast<float>(render_target_extent.height / 4),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {render_target_extent.width / 4, render_target_extent.height / 4}
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
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0xff, 0xff, 1},
            {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0xff, 0xff, 1},
            0.0f,
            1.0f
    };

    VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    std::array<VkPipelineColorBlendAttachmentState,8> pipeline_color_blend_attachment_states;
    pipeline_color_blend_attachment_states.fill(pipeline_color_blend_attachment_state);
    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            pipeline_color_blend_attachment_states.size(),
            pipeline_color_blend_attachment_states.data(),
            {0.0f,0.0f,0.0f,0.0f}
    };

    // We set the viewport and scissor dynamically
    std::array<VkDynamicState,2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamic_states.size(),
            dynamic_states.data()
    };

    // We need to tell the pipeline we will use push constants to pass the info
    VkPushConstantRange push_constant_range = {
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(glm::vec4)
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &hbao_descriptor_set_layouts[1],
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &hbao_pipelines_layouts[1]);

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
            hbao_pipelines_layouts[1],
            hbao_render_passes[1],
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &hbao_pipelines[1]);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

HbaoContext::~HbaoContext() {
    vkDeviceWaitIdle(device);
    vkDestroySampler(device, do_nothing_sampler, nullptr);

    for (auto& render_pass : hbao_render_passes) {
        vkDestroyRenderPass(device, render_pass, nullptr);
    }

    for (auto& descriptor_set_layout : hbao_descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    }

    for (auto& pipeline_layout : hbao_pipelines_layouts) {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    }

    for (auto& pipeline : hbao_pipelines) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    for (auto& framebuffer : hbao_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyImage(device, device_linearized_depth_image, nullptr);
    vkDestroyImageView(device, device_linearized_depth_image_view, nullptr);

    vkDestroyImage(device, device_deinterleaved_depth_image, nullptr);
    for (auto& image_view : device_deinterleaved_depth_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
}

std::array<VkImage, 2> HbaoContext::get_device_images() {
    return std::array<VkImage, 2> {device_linearized_depth_image, device_deinterleaved_depth_image};
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> HbaoContext::get_required_descriptor_pool_size_and_sets() {
    return {
            { {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
            },
            2};
}

void HbaoContext::create_resources(VkExtent2D screen_res) {
    this->screen_extent = screen_res;

    // Image for holding the linearized depth
    VkImageCreateInfo image_create_info = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16_SFLOAT,
        {screen_extent.width, screen_extent.height, 1},
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };
    vkDestroyImage(device, device_linearized_depth_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_linearized_depth_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // 16 layers of quarter resolution of the rendertarget
    image_create_info.extent = {screen_extent.width / 4, screen_extent.height / 4, 1};
    image_create_info.arrayLayers = 16;
    vkDestroyImage(device, device_deinterleaved_depth_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_deinterleaved_depth_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
}

void HbaoContext::init_resources() {
    // After the allocation we create the image views and the framebuffers
    create_image_views();
}

void HbaoContext::create_image_views() {
    VkImageViewCreateInfo image_view_create_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        device_linearized_depth_image,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_R16_SFLOAT,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
    };
    vkDestroyImageView(device, device_linearized_depth_image_view, nullptr);
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_linearized_depth_image_view);

    image_view_create_info.image = device_deinterleaved_depth_image;
    for (uint32_t i = 0; i < device_deinterleaved_depth_image_views.size(); i++) {
        image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 };
        vkDestroyImageView(device, device_deinterleaved_depth_image_views[i], nullptr);
        vkCreateImageView(device, &image_view_create_info, nullptr, &device_deinterleaved_depth_image_views[i]);
    }
}

void HbaoContext::create_framebuffers(VkImageView depth_image_view) {
    for (auto& framebuffer : hbao_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    // Framebuffer for the depth_linearize pass
    std::array<VkImageView, 2> input_image_views = {depth_image_view, device_linearized_depth_image_view};
    VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            hbao_render_passes[0],
            input_image_views.size(),
            input_image_views.data(),
            screen_extent.width,
            screen_extent.height,
            1
    };
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hbao_framebuffers[0]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for the deinterleave pass
    std::array<VkImageView, 8> input_image_views2;
    for (int i = 0; i < 8; i++) {
        input_image_views2[i] = device_deinterleaved_depth_image_views[i];
    }
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            hbao_render_passes[1],
            input_image_views2.size(),
            input_image_views2.data(),
            screen_extent.width / 4,
            screen_extent.height / 4,
            1
    };
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hbao_framebuffers[1]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for the deinterleave pass2
    for (int i = 8; i < 16; i++) {
        input_image_views2[i-8] = device_deinterleaved_depth_image_views[i];
    }
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            hbao_render_passes[1],
            input_image_views2.size(),
            input_image_views2.data(),
            screen_extent.width / 4,
            screen_extent.height / 4,
            1
    };
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hbao_framebuffers[2]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
}

void HbaoContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView depth_image_view) {
    create_framebuffers(depth_image_view);

    std::array<VkDescriptorSetLayout, 2> layouts_of_sets {hbao_descriptor_set_layouts[0], hbao_descriptor_set_layouts[1]};
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(layouts_of_sets.size()),
            layouts_of_sets.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, hbao_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    // We create the infos for the descriptor writes
    std::array<VkDescriptorImageInfo, 2> descriptor_image_infos;
    // For the depth_linearize
    descriptor_image_infos[0] = {
            VK_NULL_HANDLE,
            depth_image_view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    // For the deinterleave
    descriptor_image_infos[1] = {
            do_nothing_sampler,
            device_linearized_depth_image_view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    // Then we have to write the descriptor set
    std::array<VkWriteDescriptorSet, 2> write_descriptor_sets;
    write_descriptor_sets[0] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        hbao_descriptor_sets[0],
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        &descriptor_image_infos[0],
        nullptr,
        nullptr
    };
    write_descriptor_sets[1] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            hbao_descriptor_sets[1],
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[1],
            nullptr,
            nullptr
    };
    vkUpdateDescriptorSets(device, write_descriptor_sets.size(), write_descriptor_sets.data(), 0, nullptr);
}

void HbaoContext::record_into_command_buffer(VkCommandBuffer command_buffer, VkExtent2D out_image_size, float znear, float zfar, bool is_perspective) {
    // Common structures for all the binded pipelines
    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(out_image_size.width),
            static_cast<float>(out_image_size.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {out_image_size.width, out_image_size.height}
    };
    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clear_values[1].color = {0.0f, 0.0f, 0.0f, 0.0f};

    // Depth linearize pipeline
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hbao_pipelines[0]);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            hbao_render_passes[0],
            hbao_framebuffers[0],
            {{0,0},{out_image_size.width, out_image_size.height}},
            2,
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hbao_pipelines_layouts[0], 0, 1, &hbao_descriptor_sets[0], 0, nullptr);

    glm::vec4 clip_info(znear * zfar, znear - zfar, zfar, is_perspective ? 1 : 0);
    vkCmdPushConstants(command_buffer, hbao_pipelines_layouts[0], VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), glm::value_ptr(clip_info));
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // Deinterleave pass
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hbao_pipelines[1]);
    viewport = {
            0.0f,
            0.0f,
            static_cast<float>(out_image_size.width / 4),
            static_cast<float>(out_image_size.height / 4),
            0.0f,
            1.0f
    };
    scissor = {
            {0,0},
            {out_image_size.width / 4, out_image_size.height / 4}
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    for(int i = 0; i < 2; i++) {
        render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            hbao_render_passes[1],
            hbao_framebuffers[i+1],
            {{0,0},{out_image_size.width / 4, out_image_size.height / 4}},
            1,
            clear_values.data()
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hbao_pipelines_layouts[1], 0, 1, &hbao_descriptor_sets[1], 0, nullptr);

        int x = i*8;
        glm::vec4 info(float(x % 4) + 0.5f, float(x / 4) + 0.5f, 1.0f/out_image_size.width, 1.0f/out_image_size.height);
        vkCmdPushConstants(command_buffer, hbao_pipelines_layouts[1], VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), glm::value_ptr(info));
        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);
    }

}