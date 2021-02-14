#include "vsm_context.h"
#include "../vulkan_helper.h"
#include <unordered_map>
#include <utility>
#include <iostream>

VSMContext::VSMContext(VkDevice device) {
    this->device = device;

    // Creation of the sampler used to sample from the vsm images
    VkSamplerCreateInfo sampler_create_info = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            0.0f,
            VK_TRUE,
            16.0f,
            VK_FALSE,
            VK_COMPARE_OP_LESS,
            0.0f,
            1.0f,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &device_shadow_map_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    sampler_create_info = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            1,
            VK_TRUE,
            16.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0f,
            1.0f,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &device_max_aniso_linear_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    // Creation of the descriptor set layout used in the shader
    std::array<VkDescriptorSetLayoutBinding, 2> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            &device_max_aniso_linear_sampler
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            &device_max_aniso_linear_sampler
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &vsm_descriptor_set_layout), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Creation for the renderpass
    std::array<VkAttachmentDescription,2> attachment_descriptions {{
        {
            0,
            VK_FORMAT_D32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
        },
        {
            0,
            VK_FORMAT_R32G32_SFLOAT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }
    }};
    std::array<VkAttachmentReference,2> attachment_references {{
        { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
        { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    }};
    VkSubpassDescription subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
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
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &shadow_map_render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);
}

VSMContext::~VSMContext() {
    vkDestroyPipelineLayout(device, gaussian_blur_pipeline_layout, nullptr);
    for(auto& pipeline : gaussian_blur_xy_pipelines) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    vkDestroyPipelineLayout(device, shadow_map_pipeline_layout, nullptr);
    vkDestroyPipeline(device, shadow_map_pipeline, nullptr);

    for(auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(device, shadow_map_render_pass, nullptr);
    vkDestroyDescriptorSetLayout(device, vsm_descriptor_set_layout, nullptr);
    vkDestroySampler(device, device_shadow_map_sampler, nullptr);
    vkDestroySampler(device, device_max_aniso_linear_sampler, nullptr);

    for (auto& image_views : device_vsm_depth_image_views) {
        vkDestroyImageView(device, image_views[0], nullptr);
        vkDestroyImageView(device, image_views[1], nullptr);
    }
    for (auto& image : device_vsm_depth_images) {
        vkDestroyImage(device, image, nullptr);
    }

    for (auto& image_view : device_light_depth_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    for (auto &image : device_light_depth_images) {
        vkDestroyImage(device, image, nullptr);
    }
}

void VSMContext::create_resources(std::vector<VkExtent2D> depth_images_res, std::string shader_dir_path,
                                  VkDescriptorSetLayout pbr_model_set_layout, VkDescriptorSetLayout light_set_layout) {
    this->depth_images_res = depth_images_res;

    // We create two vsm depth images for every light
    device_vsm_depth_images.resize(depth_images_res.size());
    device_light_depth_images.resize(depth_images_res.size());
    for (int i=0; i<device_vsm_depth_images.size(); i++) {
        VkImageCreateInfo image_create_info = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                nullptr,
                0,
                VK_IMAGE_TYPE_2D,
                VK_FORMAT_R32G32_SFLOAT,
                {depth_images_res[i].width, depth_images_res[i].height, 1},
                1,
                2,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_SHARING_MODE_EXCLUSIVE,
                0,
                nullptr,
                VK_IMAGE_LAYOUT_UNDEFINED
        };
        check_error(vkCreateImage(device, &image_create_info, nullptr, &device_vsm_depth_images[i]), vulkan_helper::Error::IMAGE_CREATION_FAILED);

        image_create_info.format = VK_FORMAT_D32_SFLOAT;
        image_create_info.arrayLayers = 1;
        image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        check_error(vkCreateImage(device, &image_create_info, nullptr, &device_light_depth_images[i]), vulkan_helper::Error::IMAGE_CREATION_FAILED);
    }

    create_shadow_map_pipeline(shader_dir_path, pbr_model_set_layout, light_set_layout);
    create_gaussian_blur_pipelines(shader_dir_path);
}

void VSMContext::create_shadow_map_pipeline(std::string shader_dir_path, VkDescriptorSetLayout pbr_model_set_layout, VkDescriptorSetLayout light_set_layout) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//shadow_map.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//shadow_map.frag.spv", shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    std::array<VkPipelineShaderStageCreateInfo,2> pipeline_shaders_stage_create_infos {{
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
    std::array<VkVertexInputAttributeDescription,4> vertex_input_attribute_description {{
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

    // we will set the viewport and scissor dynamically after pipeline binding
    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(depth_images_res.front().width),
            static_cast<float>(depth_images_res.front().height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            depth_images_res.front()
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

    std::array<VkDynamicState,2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamic_states.size(),
            dynamic_states.data()
    };

    // We need to tell the pipeline we will use push constants to pass a uint32_t
    VkPushConstantRange push_constant_range = {
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(uint32_t)
    };
    std::array<VkDescriptorSetLayout,2> descriptor_set_layouts = { pbr_model_set_layout, light_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layouts.size(),
            descriptor_set_layouts.data(),
            1,
            &push_constant_range
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &shadow_map_pipeline_layout);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_infos.size(),
            pipeline_shaders_stage_create_infos.data(),
            &pipeline_vertex_input_state_create_info,
            &pipeline_input_assembly_create_info,
            nullptr,
            &pipeline_viewport_state_create_info,
            &pipeline_rasterization_state_create_info,
            &pipeline_multisample_state_create_info,
            &pipeline_depth_stencil_state_create_info,
            &pipeline_color_blend_state_create_info,
            &pipeline_dynamic_state_create_info,
            shadow_map_pipeline_layout,
            shadow_map_render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &shadow_map_pipeline);
    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VSMContext::create_gaussian_blur_pipelines(std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//gaussian_blur_x.comp.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    std::array<VkShaderModule,2> shader_modules;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_modules[0]), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//gaussian_blur_y.comp.spv", shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_modules[1]), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    std::array<VkPipelineShaderStageCreateInfo,2> pipeline_shaders_stage_create_infos {{
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_COMPUTE_BIT,
            shader_modules[0],
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_COMPUTE_BIT,
            shader_modules[1],
            "main",
            nullptr
        }
    }};

    std::array<VkDescriptorSetLayout,1> descriptor_set_layouts = {vsm_descriptor_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layouts.size(),
            descriptor_set_layouts.data(),
            0,
            nullptr
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &gaussian_blur_pipeline_layout);

    std::array<VkComputePipelineCreateInfo,2> compute_pipeline_create_infos {{
        {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_infos[0],
            gaussian_blur_pipeline_layout,
            VK_NULL_HANDLE,
            -1
        },
        {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_infos[1],
            gaussian_blur_pipeline_layout,
            VK_NULL_HANDLE,
            -1
        }
    }};

    vkCreateComputePipelines(device, VK_NULL_HANDLE, compute_pipeline_create_infos.size(), compute_pipeline_create_infos.data(), nullptr, gaussian_blur_xy_pipelines.data());

    for (auto& shader_module : shader_modules) {
        vkDestroyShaderModule(device, shader_module, nullptr);
    }
}

std::vector<VkImage> VSMContext::get_device_images() {
    auto vec = device_vsm_depth_images;
    vec.insert(vec.begin(), device_light_depth_images.begin(), device_light_depth_images.end());
    return vec;
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> VSMContext::get_required_descriptor_pool_size_and_sets() {
    return {
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2*device_vsm_depth_images.size()},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2*device_vsm_depth_images.size()}},
            2*device_vsm_depth_images.size()};
}

void VSMContext::create_image_views() {
    device_vsm_depth_image_views.resize(device_vsm_depth_images.size());
    device_light_depth_image_views.resize(device_light_depth_images.size());

    for (int i=0; i<device_vsm_depth_image_views.size(); i++) {
        VkImageViewCreateInfo image_view_create_info = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                nullptr,
                0,
                device_vsm_depth_images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                VK_FORMAT_R32G32_SFLOAT,
                {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
        };
        check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &device_vsm_depth_image_views[i][0]), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
        image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1, 1 };
        check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &device_vsm_depth_image_views[i][1]), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);

        image_view_create_info.image = device_light_depth_images[i];
        image_view_create_info.format = VK_FORMAT_D32_SFLOAT;
        image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &device_light_depth_image_views[i]), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
    }
}

void VSMContext::create_framebuffers() {
    // We need to create one framebuffer for each light
    framebuffers.resize(depth_images_res.size());
    for (int i=0; i<depth_images_res.size(); i++) {
        std::array<VkImageView,2> attachments {device_light_depth_image_views[i], device_vsm_depth_image_views[i][0] };
        VkFramebufferCreateInfo framebuffer_create_info = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                shadow_map_render_pass,
                attachments.size(),
                attachments.data(),
                depth_images_res[i].width,
                depth_images_res[i].height,
                1
        };
        check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[i]), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
    }
}

void VSMContext::init_resources() {
    create_image_views();
    create_framebuffers();
}

void VSMContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool) {
    // We need to create 2*images sets with the same layouts
    std::vector<VkDescriptorSetLayout> layouts_of_sets(2*device_vsm_depth_images.size(), vsm_descriptor_set_layout);
    vsm_descriptor_sets.resize(2*device_vsm_depth_images.size());
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(layouts_of_sets.size()),
            layouts_of_sets.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, vsm_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    // First 4 writes of write_descriptor_set
    std::vector<VkDescriptorImageInfo> descriptor_image_infos(4*device_vsm_depth_images.size());
    for (int i=0; i<descriptor_image_infos.size(); i++) {
        // First two are for the first pass of the blur
        descriptor_image_infos[i] = {
                device_max_aniso_linear_sampler,
                device_vsm_depth_image_views[i/4][0],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        descriptor_image_infos[++i] = {
                device_max_aniso_linear_sampler,
                device_vsm_depth_image_views[i/4][1],
                VK_IMAGE_LAYOUT_GENERAL
        };

        // Second two are for the second pass of the blur
        descriptor_image_infos[++i] = {
                device_max_aniso_linear_sampler,
                device_vsm_depth_image_views[i/4][1],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        descriptor_image_infos[++i] = {
                device_max_aniso_linear_sampler,
                device_vsm_depth_image_views[i/4][0],
                VK_IMAGE_LAYOUT_GENERAL
        };
    }

    std::vector<VkWriteDescriptorSet> write_descriptor_set(2*vsm_descriptor_sets.size());
    for (int i=0; i<write_descriptor_set.size(); i++) {
        write_descriptor_set[i] = {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    vsm_descriptor_sets[i/2],
                    0,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    &descriptor_image_infos[i],
                    nullptr,
                    nullptr
        };
        //i++;
        write_descriptor_set[++i] = {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    vsm_descriptor_sets[i/2],
                    1,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    &descriptor_image_infos[i],
                    nullptr,
                    nullptr
        };
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}

void VSMContext::record_into_command_buffer(VkCommandBuffer command_buffer, std::vector<VkDescriptorSet> object_data_sets,
                                            VkDescriptorSet light_data_set, std::vector<vulkan_helper::ObjectRenderInfo> object_render_info) {
    std::array<VkClearValue,2> clear_values;
    clear_values[0].depthStencil = {1.0f, 0};
    clear_values[1].color = {1.0f, 1.0f, 1.0f, 1.0f};

    for (uint32_t i=0; i<depth_images_res.size(); i++) {
        // We first render the shadowmap
        VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            shadow_map_render_pass,
            framebuffers[i],
            {{0,0},{depth_images_res[i]}},
            clear_values.size(),
            clear_values.data()
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline);

        VkViewport viewport = {
                0.0f,
                0.0f,
                static_cast<float>(depth_images_res[i].width),
                static_cast<float>(depth_images_res[i].height),
                0.0f,
                1.0f
        };
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor = {
                {0,0},
                depth_images_res[i]
        };
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        vkCmdPushConstants(command_buffer, shadow_map_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &i);

        for (int j=0; j<object_data_sets.size(); j++) {
            std::array<VkDescriptorSet,2> sets_to_bind {object_data_sets[j], light_data_set};
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout, 0, sets_to_bind.size(), sets_to_bind.data(), 0, nullptr);

            vkCmdBindVertexBuffers(command_buffer, 0, 1, &object_render_info[j].data_buffer, &object_render_info[j].mesh_data_offset);
            vkCmdBindIndexBuffer(command_buffer, object_render_info[j].data_buffer, object_render_info[j].index_data_offset, object_render_info[j].index_data_type);
            vkCmdDrawIndexed(command_buffer, object_render_info[j].indices, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(command_buffer);

        // Then we blur the shadow_map in the x dimension
        std::array<VkImageMemoryBarrier,2> image_memory_barriers;
        image_memory_barriers[0] = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_vsm_depth_images[i],
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 }
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipelines[0]);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_pipeline_layout, 0, 1, &vsm_descriptor_sets[2*i+0], 0, nullptr);
        vkCmdDispatch(command_buffer, std::ceil(depth_images_res[i].width/32.0f), std::ceil(depth_images_res[i].height/32.0f), 1);

        // Then we blur in the y dimension
        image_memory_barriers[0] = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_vsm_depth_images[i],
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 }
        };
        image_memory_barriers[1] = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_vsm_depth_images[i],
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 }
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, image_memory_barriers.data());

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipelines[1]);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_pipeline_layout, 0, 1, &vsm_descriptor_sets[2*i+1], 0, nullptr);
        vkCmdDispatch(command_buffer, std::ceil(depth_images_res[i].width/32.0f), std::ceil(depth_images_res[i].height/32.0f), 1);

        image_memory_barriers[0] = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_vsm_depth_images[i],
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 }
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());
    }
}

VkImageView VSMContext::get_image_view(int index) {
    return device_vsm_depth_image_views.at(index)[0];
}