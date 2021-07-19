#include "hbao_context.h"

HbaoContext::HbaoContext(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties, VkExtent2D screen_res,
                         VkFormat depth_image_format, VkFormat out_ao_image_format, std::string shader_dir_path, bool generate_normals) :
                        distribution(0.0f, 1.0f) {
    this->device = device;
    this->physical_device_memory_properties = memory_properties;
    this->generate_normals = generate_normals;

    // Sampler that filters by nearest
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

    std::array<VkAttachmentDescription, 8> attachment_descriptions;
    std::array<VkAttachmentReference,8> attachment_references;
    // Render pass for depth_linearize
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
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[DEPTH_LINEARIZE].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // Renderpass for view_normal
    if (generate_normals) {
        attachment_descriptions[0] = {
            0,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        attachment_references[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
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
            1,
            attachment_descriptions.data(),
            1,
            &subpass_description,
            0,
            nullptr
        };
        check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[VIEW_NORMAL].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);
    }

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
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[DEPTH_DEINTERLEAVE].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // Renderpass for hbao_calc
    attachment_descriptions[0] = {
            0,
            VK_FORMAT_R16G16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    attachment_references[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &attachment_references[0],
            nullptr,
            nullptr,
            0,
            nullptr
    };
    uint32_t viewMask = 0xffff; // We write to 16 images 0xffff = 0b1111111111111111
    uint32_t correlationMask = 0xffff;
    VkRenderPassMultiviewCreateInfo renderPassMultiviewCI = {
            VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
            nullptr,
            1,
            &viewMask,
            0,
            nullptr,
            1,
            &correlationMask
    };
    render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            &renderPassMultiviewCI,
            0,
            1,
            &attachment_descriptions[0],
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[HBAO_CALC].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // Renderpass for reinterleave
    // Output linearized depth image is of the format VK_FORMAT_R16G16_SFLOAT
    attachment_descriptions[0] = {
            0,
            VK_FORMAT_R16G16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    attachment_references[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
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
            1,
            attachment_descriptions.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[HBAO_REINTERLEAVE].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // Renderpass for hbao_blur
    // Output linearized depth image is of the format VK_FORMAT_R16G16_SFLOAT
    attachment_descriptions[0] = {
            0,
            VK_FORMAT_R16G16_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    attachment_references[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
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
            1,
            attachment_descriptions.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[HBAO_BLUR].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    // Renderpass for hbao_blur_2
    // Output linearized depth image is of the format VK_FORMAT_R16G16_SFLOAT
    attachment_descriptions[0] = {
            0,
            out_ao_image_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    attachment_references[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
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
            1,
            attachment_descriptions.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &stages[HBAO_BLUR_2].render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);

    std::array<VkDescriptorSetLayoutBinding, 3> descriptor_set_layout_binding;
    // We create the descriptor set layout for the depth linearize
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
            1,
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hbao_descriptor_set_layouts[0]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // view_normal set layout
    if (generate_normals) {
        descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
        };
        descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
        };
        descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            2,
            descriptor_set_layout_binding.data()
        };
        check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hbao_descriptor_set_layouts[1]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);
    }

    // We create the descriptor set layout for the deinterleave, reinterleave, hbao_blur and hbao_blur2
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
            1,
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hbao_descriptor_set_layouts[2]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // We create the descriptor set layout for the hbao_calc
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_binding[2] = {
            2,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            3,
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hbao_descriptor_set_layouts[3]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    stages[DEPTH_LINEARIZE].descriptor_set_layout = &hbao_descriptor_set_layouts[0];
    stages[VIEW_NORMAL].descriptor_set_layout = &hbao_descriptor_set_layouts[1];
    stages[DEPTH_DEINTERLEAVE].descriptor_set_layout = &hbao_descriptor_set_layouts[2];
    stages[HBAO_CALC].descriptor_set_layout = &hbao_descriptor_set_layouts[3];
    stages[HBAO_REINTERLEAVE].descriptor_set_layout = &hbao_descriptor_set_layouts[2];
    stages[HBAO_BLUR].descriptor_set_layout = &hbao_descriptor_set_layouts[2];
    stages[HBAO_BLUR_2].descriptor_set_layout = &hbao_descriptor_set_layouts[2];

    // Then we start creating the pipelines, by creating the structures that are common for all the pipelines
    pipeline_common_structures structures;
    fill_full_screen_pipeline_common_structures(shader_dir_path + "//fullscreen_tri.vert.spv", structures);

    // Creating the pipelines
    create_linearize_depth_pipeline(structures, screen_res,  shader_dir_path + "//depth_linearize.frag.spv");
    if (generate_normals) {
        create_view_normal_pipeline(structures, screen_res, shader_dir_path + "//view_normal.frag.spv");
    }
    create_deinterleave_pipeline(structures, screen_res, shader_dir_path + "//hbao_deinterleave.frag.spv");
    create_hbao_calc_pipeline(structures, screen_res, shader_dir_path + "//hbao_calc.frag.spv");
    create_reinterleave_pipeline(structures, screen_res, shader_dir_path  + "//hbao_reinterleave.frag.spv");
    create_hbao_blur_pipeline(structures, screen_res, shader_dir_path  + "//hbao_blur.frag.spv");
    create_hbao_blur_2_pipeline(structures, screen_res, shader_dir_path  + "//hbao_blur_2.frag.spv");

    // We don't need the vertex shader module anymore
    vkDestroyShaderModule(device, structures.vertex_shader_stage.module, nullptr);

    create_hbao_data_buffers();
}

void HbaoContext::fill_full_screen_pipeline_common_structures(std::string vertex_shader_path, pipeline_common_structures &structures) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(vertex_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    structures.vertex_shader_stage = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_VERTEX_BIT,
        vertex_shader_module,
        "main",
        nullptr
    };

    structures.vertex_input = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            0,
            nullptr,
            0,
            nullptr
    };

    structures.input_assembly = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_FALSE
    };

    structures.rasterization = {
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

    structures.multisample = {
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

    structures.depth_stencil = {
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

    structures.color_blend_attachment = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
}

void HbaoContext::create_linearize_depth_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        fragment_shader_module,
        "main",
        nullptr
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

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &structures.color_blend_attachment,
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
            stages[DEPTH_LINEARIZE].descriptor_set_layout,
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[DEPTH_LINEARIZE].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[DEPTH_LINEARIZE].pipeline_layout,
            stages[DEPTH_LINEARIZE].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[DEPTH_LINEARIZE].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_view_normal_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader_module,
            "main",
            nullptr
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

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &structures.color_blend_attachment,
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

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            stages[VIEW_NORMAL].descriptor_set_layout,
            0,
            nullptr
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[VIEW_NORMAL].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[VIEW_NORMAL].pipeline_layout,
            stages[VIEW_NORMAL].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[VIEW_NORMAL].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_deinterleave_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        fragment_shader_module,
        "main",
        nullptr
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

    std::array<VkPipelineColorBlendAttachmentState,8> pipeline_color_blend_attachment_states;
    pipeline_color_blend_attachment_states.fill(structures.color_blend_attachment);
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
            stages[DEPTH_DEINTERLEAVE].descriptor_set_layout,
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[DEPTH_DEINTERLEAVE].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[DEPTH_DEINTERLEAVE].pipeline_layout,
            stages[DEPTH_DEINTERLEAVE].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[DEPTH_DEINTERLEAVE].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_hbao_calc_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t *>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module),vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        fragment_shader_module,
        "main",
        nullptr
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

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &structures.color_blend_attachment,
            {0.0f, 0.0f, 0.0f, 0.0f}
    };

    // We set the viewport and scissor dynamically
    std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamic_states.size(),
            dynamic_states.data()
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            stages[HBAO_CALC].descriptor_set_layout,
            0,
            nullptr
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[HBAO_CALC].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[HBAO_CALC].pipeline_layout,
            stages[HBAO_CALC].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[HBAO_CALC].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_reinterleave_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader_module,
            "main",
            nullptr
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

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &structures.color_blend_attachment,
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

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            stages[HBAO_REINTERLEAVE].descriptor_set_layout,
            0,
            nullptr
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[HBAO_REINTERLEAVE].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[HBAO_REINTERLEAVE].pipeline_layout,
            stages[HBAO_REINTERLEAVE].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[HBAO_REINTERLEAVE].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_hbao_blur_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader_module,
            "main",
            nullptr
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

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &structures.color_blend_attachment,
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
            sizeof(glm::vec3)
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            stages[HBAO_BLUR].descriptor_set_layout,
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[HBAO_BLUR].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[HBAO_BLUR].pipeline_layout,
            stages[HBAO_BLUR].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[HBAO_BLUR].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_hbao_blur_2_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(frag_shader_path, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    std::array<VkPipelineShaderStageCreateInfo, 2> pipeline_shaders_stage_create_info;
    pipeline_shaders_stage_create_info[0] = structures.vertex_shader_stage;
    pipeline_shaders_stage_create_info[1] = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader_module,
            "main",
            nullptr
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

    VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &structures.color_blend_attachment,
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
            sizeof(glm::vec3)
    };
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            stages[HBAO_BLUR_2].descriptor_set_layout,
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &stages[HBAO_BLUR_2].pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &structures.vertex_input,
            &structures.input_assembly,
            nullptr,
            &pipeline_viewport_state_create_info,
            &structures.rasterization,
            &structures.multisample,
            &structures.depth_stencil,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            stages[HBAO_BLUR_2].pipeline_layout,
            stages[HBAO_BLUR_2].render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &stages[HBAO_BLUR_2].pipeline);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void HbaoContext::create_hbao_data_buffers() {
    // Buffer and allocation for the host memory part of HbaoData
    VkBufferCreateInfo buffer_create_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        sizeof(HbaoData),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    vkCreateBuffer(device, &buffer_create_info, nullptr, &host_hbao_data_buffer);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, host_hbao_data_buffer, &memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            memory_requirements.size,
            vulkan_helper::select_memory_index(physical_device_memory_properties, memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &host_hbao_data_memory), vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    vkBindBufferMemory(device, host_hbao_data_buffer, host_hbao_data_memory, 0);

    void *ptr;
    vkMapMemory(device, host_hbao_data_memory, 0, sizeof(HbaoData), 0, &ptr);
    hbao_data = reinterpret_cast<HbaoData*>(ptr);

    // Buffer for the device memory part of HbaoData
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vkCreateBuffer(device, &buffer_create_info, nullptr, &device_hbao_data_buffer);

    vkGetBufferMemoryRequirements(device, device_hbao_data_buffer, &memory_requirements);

    memory_allocate_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            memory_requirements.size,
            vulkan_helper::select_memory_index(physical_device_memory_properties, memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &device_hbao_data_memory), vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    vkBindBufferMemory(device, device_hbao_data_buffer, device_hbao_data_memory, 0);
}

HbaoContext::~HbaoContext() {
    vkDeviceWaitIdle(device);
    vkDestroySampler(device, do_nothing_sampler, nullptr);

    for (auto& set_layout : hbao_descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
    }

    for (auto& stage : stages) {
        vkDestroyRenderPass(device, stage.render_pass, nullptr);
        vkDestroyPipelineLayout(device, stage.pipeline_layout, nullptr);
        vkDestroyPipeline(device, stage.pipeline, nullptr);
    }

    vkDestroyFramebuffer(device, depth_linearize_framebuffer, nullptr);
    vkDestroyFramebuffer(device, view_normal_framebuffer, nullptr);
    for (auto& framebuffer : deinterleave_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    vkDestroyFramebuffer(device, hbao_calc_framebuffer, nullptr);
    vkDestroyFramebuffer(device, reinterleave_framebuffer, nullptr);
    for (auto& framebuffer : hbao_blur_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyImage(device, device_linearized_depth_image, nullptr);
    vkDestroyImageView(device, device_linearized_depth_image_view, nullptr);

    vkDestroyImage(device, device_view_space_normal_image, nullptr);
    vkDestroyImageView(device, device_view_space_normal_image_view, nullptr);

    vkDestroyImage(device, device_deinterleaved_depth_image, nullptr);
    vkDestroyImageView(device, device_deinterleaved_depth_16_layers_image_view, nullptr);
    for (auto& image_view : device_deinterleaved_depth_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }

    vkDestroyImage(device, device_hbao_calc_image, nullptr);
    vkDestroyImageView(device, device_hbao_calc_16_layers_image_view, nullptr);

    vkDestroyImage(device, device_reinterleaved_hbao_image, nullptr);
    for (auto &image_view : device_reinterleaved_hbao_image_view) {
        vkDestroyImageView(device, image_view, nullptr);
    }

    vkDestroyBuffer(device, device_hbao_data_buffer, nullptr);
    vkFreeMemory(device, device_hbao_data_memory, nullptr);

    vkDestroyBuffer(device, host_hbao_data_buffer, nullptr);
    vkFreeMemory(device, host_hbao_data_memory, nullptr);
}

std::vector<VkImage> HbaoContext::get_device_images() {
    std::vector<VkImage> images_to_allocate = {device_linearized_depth_image, device_deinterleaved_depth_image, device_hbao_calc_image, device_reinterleaved_hbao_image};
    if (generate_normals) {
        images_to_allocate.push_back(device_view_space_normal_image);
    }
    return images_to_allocate;
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> HbaoContext::get_required_descriptor_pool_size_and_sets() {
    std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> to_return {
            { {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6}
            },
            6};

    if (generate_normals) {
        to_return.first[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] += 1;
        to_return.first[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] += 1;
        to_return.second += 1;
    }
    return to_return;
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

    // image for the reconstructed normals
    if (generate_normals) {
        image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_create_info.extent = {screen_extent.width, screen_extent.height, 1},
        vkDestroyImage(device, device_view_space_normal_image, nullptr);
        check_error(vkCreateImage(device, &image_create_info, nullptr, &device_view_space_normal_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
    }

    // 16 layers of quarter resolution of the rendertarget
    image_create_info.format = VK_FORMAT_R16_SFLOAT;
    image_create_info.extent = {screen_extent.width / 4, screen_extent.height / 4, 1};
    image_create_info.arrayLayers = 16;
    vkDestroyImage(device, device_deinterleaved_depth_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_deinterleaved_depth_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // 16 layers R16G16 of quarter resolution of the rendertarget
    image_create_info.format = VK_FORMAT_R16G16_SFLOAT;
    vkDestroyImage(device, device_hbao_calc_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_hbao_calc_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // image for the reinterleaved output of the hbao_calc pass and second layer for hbao_blur
    image_create_info.format = VK_FORMAT_R16G16_SFLOAT;
    image_create_info.extent = {screen_extent.width, screen_extent.height, 1},
    image_create_info.arrayLayers = 2;
    vkDestroyImage(device, device_reinterleaved_hbao_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_reinterleaved_hbao_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
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

    // image view for the reconstructed normals
    if (generate_normals) {
        image_view_create_info.image = device_view_space_normal_image;
        image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkDestroyImageView(device, device_view_space_normal_image_view, nullptr);
        vkCreateImageView(device, &image_view_create_info, nullptr, &device_view_space_normal_image_view);
    }

    // 16 image views one for each layer of depth_linearize_image
    image_view_create_info.image = device_deinterleaved_depth_image;
    image_view_create_info.format = VK_FORMAT_R16_SFLOAT;
    for (uint32_t i = 0; i < device_deinterleaved_depth_image_views.size(); i++) {
        image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 };
        vkDestroyImageView(device, device_deinterleaved_depth_image_views[i], nullptr);
        vkCreateImageView(device, &image_view_create_info, nullptr, &device_deinterleaved_depth_image_views[i]);
    }

    // 1 image view for all 16 layers of depth_linearize_image
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 16};
    vkDestroyImageView(device, device_deinterleaved_depth_16_layers_image_view, nullptr);
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_deinterleaved_depth_16_layers_image_view);

    // 1 image view for all 16 layers of hbao_calc_image
    image_view_create_info.image = device_hbao_calc_image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    image_view_create_info.format = VK_FORMAT_R16G16_SFLOAT;
    vkDestroyImageView(device, device_hbao_calc_16_layers_image_view, nullptr);
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_hbao_calc_16_layers_image_view);

    // image view for the reinterleaved hbao calc image layer 0
    image_view_create_info.image = device_reinterleaved_hbao_image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = VK_FORMAT_R16G16_SFLOAT;

    for (uint32_t i = 0; i < device_reinterleaved_hbao_image_view.size(); i++) {
        image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1};
        vkDestroyImageView(device, device_reinterleaved_hbao_image_view[i], nullptr);
        vkCreateImageView(device, &image_view_create_info, nullptr, &device_reinterleaved_hbao_image_view[i]);
    }
}

void HbaoContext::create_framebuffers(VkImageView depth_image_view, VkImageView out_ao_image_view) {
    std::array<VkImageView, 8> input_image_views;

    // Framebuffer for the depth_linearize pass
    input_image_views[0] = depth_image_view;
    input_image_views[1] = device_linearized_depth_image_view;
    VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[DEPTH_LINEARIZE].render_pass,
            2,
            input_image_views.data(),
            screen_extent.width,
            screen_extent.height,
            1
    };
    vkDestroyFramebuffer(device, depth_linearize_framebuffer, nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &depth_linearize_framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for view_normal
    if (generate_normals) {
        input_image_views[0] = {device_view_space_normal_image_view};
        framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[VIEW_NORMAL].render_pass,
            1,
            input_image_views.data(),
            screen_extent.width,
            screen_extent.height,
            1
        };
        vkDestroyFramebuffer(device, view_normal_framebuffer, nullptr);
        check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &view_normal_framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
    }

    // Framebuffer for the deinterleave pass with the first 8 image views
    for (auto& framebuffer : deinterleave_framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (int i = 0; i < 8; i++) {
        input_image_views[i] = device_deinterleaved_depth_image_views[i];
    }
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[DEPTH_DEINTERLEAVE].render_pass,
            8,
            input_image_views.data(),
            screen_extent.width / 4,
            screen_extent.height / 4,
            1
    };
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &deinterleave_framebuffers[0]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for the deinterleave pass2 with the last 8 remaining image views
    for (int i = 0; i < 8; i++) {
        input_image_views[i] = device_deinterleaved_depth_image_views[i+8];
    }
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &deinterleave_framebuffers[1]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for hbao_calc
    input_image_views[0] = {device_hbao_calc_16_layers_image_view};
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[HBAO_CALC].render_pass,
            1,
            input_image_views.data(),
            screen_extent.width / 4,
            screen_extent.height / 4,
            1
    };
    vkDestroyFramebuffer(device, hbao_calc_framebuffer, nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hbao_calc_framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for hbao_reinterleave
    input_image_views[0] = {device_reinterleaved_hbao_image_view[0]};
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[HBAO_REINTERLEAVE].render_pass,
            1,
            input_image_views.data(),
            screen_extent.width,
            screen_extent.height,
            1
    };
    vkDestroyFramebuffer(device, reinterleave_framebuffer, nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &reinterleave_framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for hbao_blur
    input_image_views[0] = {device_reinterleaved_hbao_image_view[1]};
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[HBAO_BLUR].render_pass,
            1,
            input_image_views.data(),
            screen_extent.width,
            screen_extent.height,
            1
    };
    vkDestroyFramebuffer(device, hbao_blur_framebuffers[0], nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hbao_blur_framebuffers[0]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);

    // Framebuffer for hbao_blur_2
    input_image_views[0] = {out_ao_image_view};
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            stages[HBAO_BLUR_2].render_pass,
            1,
            input_image_views.data(),
            screen_extent.width,
            screen_extent.height,
            1
    };
    vkDestroyFramebuffer(device, hbao_blur_framebuffers[1], nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &hbao_blur_framebuffers[1]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
}

void HbaoContext::update_constants(glm::mat4 proj) {
    glm::vec4 projInfoPerspective = {
            2.0f / (proj[0][0]),                  // (x) * (R - L)/N
            2.0f / (proj[1][1]),                  // (y) * (T - B)/N
            -(1.0f - proj[2][0]) / proj[0][0],  // L/N
            -(1.0f + proj[2][0]) / proj[1][1],  // B/N
    };

    glm::vec4 projInfoOrtho = {
            2.0f / (proj[0][0]),                  // ((x) * R - L)
            2.0f / (proj[1][1]),                  // ((y) * T - B)
            -(1.0f + proj[3][0]) / proj[0][0],  // L
            -(1.0f - proj[3][1]) / proj[1][1],  // B
    };

    int useOrtho        =  static_cast<int>(proj[3][3]); // 1 if proj is ortho, 0 otherwise
    hbao_data->projOrtho = useOrtho;
    hbao_data->projInfo  = useOrtho ? projInfoOrtho : projInfoPerspective;

    float projScale;
    if(useOrtho) {
        projScale = float(screen_extent.height) / (projInfoOrtho[1]);
    }
    else {
        float fov = 2*atan(1.0f/proj[1][1]);
        projScale = float(screen_extent.height) / (tanf(fov * 0.5f) * 2.0f); // term after the division can be substituted to 2/proj[1][1]
    }

    // radius
    float blur_intensity = 2.0f;
    float bias = 0.1f;
    blur_sharpness = 40.0f;

    float meters2viewspace   = 1.0f;
    float R                  = blur_intensity * meters2viewspace;
    hbao_data->R2             = R * R;
    hbao_data->NegInvR2       = -1.0f / hbao_data->R2;
    hbao_data->RadiusToScreen = R * 0.5f * projScale;

    // ao
    hbao_data->PowExponent  = std::max(blur_intensity, 0.0f);
    hbao_data->NDotVBias    = std::min(std::max(0.0f, bias), 1.0f);
    hbao_data->AOMultiplier = 1.0f / (1.0f - hbao_data->NDotVBias);

    // resolution
    int quarterWidth  = ((screen_extent.width + 3) / 4);
    int quarterHeight = ((screen_extent.height + 3) / 4);

    hbao_data->InvQuarterResolution = glm::vec2(1.0f / float(quarterWidth), 1.0f / float(quarterHeight));
    hbao_data->InvFullResolution    = glm::vec2(1.0f / float(screen_extent.width), 1.0f / float(screen_extent.height));

    for(int i = 0; i < 16; i++) {
        hbao_data->float2Offsets[i] = glm::vec4(float(i % 4) + 0.5f, float(i / 4) + 0.5f, 0.0f, 1.0f);

        float Rand1 = distribution(random_engine);
        float Rand2 = distribution(random_engine);
        // Use random rotation angles in [0,2PI/NUM_DIRECTIONS)
        float Angle = 2.f * glm::pi<float>() * Rand1 / 8; // num_directions = 8
        hbao_data->jitters[i] = glm::vec4(cosf(Angle), sinf(Angle), Rand2, 0);
    }
}

void HbaoContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView depth_image_view, VkImageView normal_g_buffer_image_view, VkImageView out_ao_image_view) {
    create_framebuffers(depth_image_view, out_ao_image_view);

    std::vector<VkDescriptorSetLayout> descriptor_sets_to_allocate;
    for (size_t i = 0; i < stages.size(); i++) {
        if (*stages[i].descriptor_set_layout != VK_NULL_HANDLE) {
            descriptor_sets_to_allocate.push_back(*stages[i].descriptor_set_layout);
        }
    }

    std::vector<VkDescriptorSet> hbao_descriptor_sets(descriptor_sets_to_allocate.size());
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(descriptor_sets_to_allocate.size()),
            descriptor_sets_to_allocate.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, hbao_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    for (uint32_t sets_idx = 0, stages_idx = 0; sets_idx < hbao_descriptor_sets.size() || stages_idx < stages.size();) {
        if (*stages[stages_idx].descriptor_set_layout != VK_NULL_HANDLE) {
            stages[stages_idx].descriptor_set = hbao_descriptor_sets[sets_idx];
            sets_idx++;
            stages_idx++;
        }
        else {
            stages_idx++;
        }
    }

    // We create the infos for the descriptor writes
    std::vector<VkDescriptorImageInfo> descriptor_image_infos(generate_normals ? 8 : 7);
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
    // For the hbao_calc
    descriptor_image_infos[2] = {
            do_nothing_sampler,
            device_deinterleaved_depth_16_layers_image_view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    descriptor_image_infos[3] = {
            do_nothing_sampler,
            generate_normals ? device_view_space_normal_image_view : normal_g_buffer_image_view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    // For the reinterleave
    descriptor_image_infos[4] = {
            do_nothing_sampler,
            device_hbao_calc_16_layers_image_view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    // for hbao_blur
    descriptor_image_infos[5] = {
            do_nothing_sampler,
            device_reinterleaved_hbao_image_view[0],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    // for hbao_blur_2
    descriptor_image_infos[6] = {
            do_nothing_sampler,
            device_reinterleaved_hbao_image_view[1],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    // For view_normal
    if (generate_normals) {
        descriptor_image_infos[7] = {
                do_nothing_sampler,
                device_linearized_depth_image_view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    VkDescriptorBufferInfo descriptor_buffer_info = {
        device_hbao_data_buffer,
        0,
        sizeof(HbaoData)
    };

    // Then we have to write the descriptor set
    std::vector<VkWriteDescriptorSet> write_descriptor_sets(generate_normals ? 10 : 8);

    // Update for depth_linearize
    write_descriptor_sets[0] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        stages[DEPTH_LINEARIZE].descriptor_set,
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        &descriptor_image_infos[0],
        nullptr,
        nullptr
    };
    // Update for deinterleave
    write_descriptor_sets[1] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[DEPTH_DEINTERLEAVE].descriptor_set,
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[1],
            nullptr,
            nullptr
    };
    // Updates for the hbao_calc
    write_descriptor_sets[2] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[HBAO_CALC].descriptor_set,
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            nullptr,
            &descriptor_buffer_info,
            nullptr
    };
    write_descriptor_sets[3] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[HBAO_CALC].descriptor_set,
            1,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[2],
            nullptr,
            nullptr
    };
    write_descriptor_sets[4] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[HBAO_CALC].descriptor_set,
            2,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[3],
            nullptr,
            nullptr
    };
    // updates for reinterleave
    write_descriptor_sets[5] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[HBAO_REINTERLEAVE].descriptor_set,
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[4],
            nullptr,
            nullptr
    };
    // updates for hbao_blur
    write_descriptor_sets[6] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[HBAO_BLUR].descriptor_set,
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[5],
            nullptr,
            nullptr
    };
    // updates for hbao_blur_2
    write_descriptor_sets[7] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            stages[HBAO_BLUR_2].descriptor_set,
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[6],
            nullptr,
            nullptr
    };
    if (generate_normals) {
        // updates for view_normal
        write_descriptor_sets[8] = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                stages[VIEW_NORMAL].descriptor_set,
                0,
                0,
                1,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                nullptr,
                &descriptor_buffer_info,
                nullptr
        };
        write_descriptor_sets[9] = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                stages[VIEW_NORMAL].descriptor_set,
                1,
                0,
                1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                &descriptor_image_infos[7],
                nullptr,
                nullptr
        };
    }

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
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[DEPTH_LINEARIZE].pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            stages[DEPTH_LINEARIZE].render_pass,
            depth_linearize_framebuffer,
            {{0,0},{out_image_size.width, out_image_size.height}},
            2,
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[DEPTH_LINEARIZE].pipeline_layout, 0, 1, &stages[DEPTH_LINEARIZE].descriptor_set, 0, nullptr);

    glm::vec4 clip_info(znear * zfar, znear - zfar, zfar, is_perspective ? 1 : 0);
    vkCmdPushConstants(command_buffer, stages[DEPTH_LINEARIZE].pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), glm::value_ptr(clip_info));
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // view_normal pass
    // We copy the constants

    VkBufferCopy buffer_copy = { 0,0, sizeof(HbaoData)};
    vkCmdCopyBuffer(command_buffer, host_hbao_data_buffer, device_hbao_data_buffer, 1, &buffer_copy);

    // Transitioning layout from the write to the shader read
    VkBufferMemoryBarrier buffer_memory_barrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_UNIFORM_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            device_hbao_data_buffer,
            0,
            VK_WHOLE_SIZE
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);

    if (generate_normals) {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[VIEW_NORMAL].pipeline);
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            stages[VIEW_NORMAL].render_pass,
            view_normal_framebuffer,
            {{0,0},{out_image_size.width, out_image_size.height}},
            1,
            clear_values.data()
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[VIEW_NORMAL].pipeline_layout, 0, 1, &stages[VIEW_NORMAL].descriptor_set, 0, nullptr);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(command_buffer);
    }

    // Deinterleave pass
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[DEPTH_DEINTERLEAVE].pipeline);
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
            stages[DEPTH_DEINTERLEAVE].render_pass,
            deinterleave_framebuffers[i],
            {{0,0},{out_image_size.width / 4, out_image_size.height / 4}},
            1,
            clear_values.data()
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[DEPTH_DEINTERLEAVE].pipeline_layout, 0, 1, &stages[DEPTH_DEINTERLEAVE].descriptor_set, 0, nullptr);

        int x = i*8;
        glm::vec4 info(float(x % 4) + 0.5f, float(x / 4) + 0.5f, 1.0f/out_image_size.width, 1.0f/out_image_size.height);
        vkCmdPushConstants(command_buffer, stages[DEPTH_DEINTERLEAVE].pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), glm::value_ptr(info));
        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);
    }

    // hbao_calc pass
    // updating the constants
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_CALC].pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            stages[HBAO_CALC].render_pass,
            hbao_calc_framebuffer,
            {{0,0},{out_image_size.width / 4, out_image_size.height / 4}},
            1,
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_CALC].pipeline_layout, 0, 1, &stages[HBAO_CALC].descriptor_set, 0, nullptr);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // reinterleave pass
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_REINTERLEAVE].pipeline);

    viewport = {
            0.0f,
            0.0f,
            static_cast<float>(out_image_size.width),
            static_cast<float>(out_image_size.height),
            0.0f,
            1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    scissor = {
            {0,0},
            {out_image_size.width, out_image_size.height}
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            stages[HBAO_REINTERLEAVE].render_pass,
            reinterleave_framebuffer,
            {{0,0},{out_image_size.width, out_image_size.height}},
            1,
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_REINTERLEAVE].pipeline_layout, 0, 1, &stages[HBAO_REINTERLEAVE].descriptor_set, 0, nullptr);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // hbao_blur
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_BLUR].pipeline);

    viewport = {
            0.0f,
            0.0f,
            static_cast<float>(out_image_size.width),
            static_cast<float>(out_image_size.height),
            0.0f,
            1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    scissor = {
            {0,0},
            {out_image_size.width, out_image_size.height}
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            stages[HBAO_BLUR].render_pass,
            hbao_blur_framebuffers[0],
            {{0,0},{out_image_size.width, out_image_size.height}},
            1,
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_BLUR].pipeline_layout, 0, 1, &stages[HBAO_BLUR].descriptor_set, 0, nullptr);

    glm::vec3 hbao_blur_info(1.0f / screen_extent.width, 0.0f, blur_sharpness);
    vkCmdPushConstants(command_buffer, stages[HBAO_BLUR].pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec3), glm::value_ptr(hbao_blur_info));

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // hbao_blur_2
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_BLUR_2].pipeline);

    viewport = {
            0.0f,
            0.0f,
            static_cast<float>(out_image_size.width),
            static_cast<float>(out_image_size.height),
            0.0f,
            1.0f
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    scissor = {
            {0,0},
            {out_image_size.width, out_image_size.height}
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            stages[HBAO_BLUR_2].render_pass,
            hbao_blur_framebuffers[1],
            {{0,0},{out_image_size.width, out_image_size.height}},
            1,
            clear_values.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, stages[HBAO_BLUR_2].pipeline_layout, 0, 1, &stages[HBAO_BLUR_2].descriptor_set, 0, nullptr);

    glm::vec3 hbao_blur_info_2(0.0f, 1.0f / screen_extent.height, blur_sharpness);
    vkCmdPushConstants(command_buffer, stages[HBAO_BLUR_2].pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec3), glm::value_ptr(hbao_blur_info_2));

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
}