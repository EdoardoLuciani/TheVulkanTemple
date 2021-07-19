#include "vsm_context.h"
#include "../../vulkan_helper.h"
#include "../../gltf_model.h"
#include <unordered_map>
#include <utility>
#include <iostream>

VSMContext::VSMContext(VkDevice device, std::string shader_dir_path) {
    this->device = device;
    this->shader_dir_path = shader_dir_path;

    // Creation of the sampler used to sample from the vsm images
    VkSamplerCreateInfo sampler_create_info = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
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
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,//VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &device_render_target_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    // Creation of the descriptor set layout used in the shader
    std::array<VkDescriptorSetLayoutBinding, 2> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            &device_render_target_sampler
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            &device_render_target_sampler
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
    vkDeviceWaitIdle(device);

    vkDestroyPipelineLayout(device, gaussian_blur_pipeline_layout, nullptr);
    for(auto& pipeline : gaussian_blur_xy_pipelines) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    vkDestroyPipelineLayout(device, shadow_map_pipeline_layout, nullptr);
    vkDestroyPipeline(device, shadow_map_pipeline, nullptr);

    for (auto& light_vsm : lights_vsm) {
        vkDestroyFramebuffer(device, light_vsm.framebuffer, nullptr);
    }
    vkDestroyRenderPass(device, shadow_map_render_pass, nullptr);
    vkDestroyDescriptorSetLayout(device, vsm_descriptor_set_layout, nullptr);
    vkDestroySampler(device, device_render_target_sampler, nullptr);

    for (auto& light_vsm : lights_vsm) {
        vkDestroyImageView(device, light_vsm.device_vsm_depth_image_views[0], nullptr);
        vkDestroyImageView(device, light_vsm.device_vsm_depth_image_views[1], nullptr);
        vkDestroyImage(device, light_vsm.device_vsm_depth_image, nullptr);

        vkDestroyImageView(device, light_vsm.device_light_depth_image_view, nullptr);
        vkDestroyImage(device, light_vsm.device_light_depth_image, nullptr);
    }
}

void VSMContext::create_resources(std::vector<VkExtent2D> depth_images_res, std::vector<uint32_t> ssbo_indices,
                                  VkDescriptorSetLayout pbr_model_set_layout, VkDescriptorSetLayout light_set_layout) {
    for (auto& light_vsm : lights_vsm) {
        vkDestroyImage(device, light_vsm.device_vsm_depth_image, nullptr);
        vkDestroyImage(device, light_vsm.device_light_depth_image, nullptr);
    }
    lights_vsm.resize(depth_images_res.size());

    // We create two vsm depth images for every light
    for (uint32_t i=0; i < lights_vsm.size(); i++) {
        lights_vsm[i].depth_image_res = depth_images_res[i];
        lights_vsm[i].ssbo_index = ssbo_indices[i];

        VkImageCreateInfo image_create_info = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                nullptr,
                0,
                VK_IMAGE_TYPE_2D,
                VK_FORMAT_R32G32_SFLOAT,
                {lights_vsm[i].depth_image_res.width, lights_vsm[i].depth_image_res.height, 1},
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
        check_error(vkCreateImage(device, &image_create_info, nullptr, &lights_vsm[i].device_vsm_depth_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

        image_create_info.format = VK_FORMAT_D32_SFLOAT;
        image_create_info.arrayLayers = 1;
        image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        check_error(vkCreateImage(device, &image_create_info, nullptr, &lights_vsm[i].device_light_depth_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
    }

    create_shadow_map_pipeline(pbr_model_set_layout, light_set_layout);

    if (gaussian_blur_xy_pipelines[0] == VK_NULL_HANDLE || gaussian_blur_xy_pipelines[1] == VK_NULL_HANDLE) {
        create_gaussian_blur_pipelines(shader_dir_path);
    }
}

void VSMContext::create_shadow_map_pipeline(VkDescriptorSetLayout pbr_model_set_layout, VkDescriptorSetLayout light_set_layout) {
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
            static_cast<float>(lights_vsm.front().depth_image_res.width),
            static_cast<float>(lights_vsm.front().depth_image_res.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            lights_vsm.front().depth_image_res
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
    vkDestroyPipelineLayout(device, shadow_map_pipeline_layout, nullptr);
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
    vkDestroyPipeline(device, shadow_map_pipeline, nullptr);
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
    std::vector<VkImage> out_images;
    out_images.reserve(lights_vsm.size()*2);
    for (const auto& light_vsm : lights_vsm) {
        out_images.push_back(light_vsm.device_vsm_depth_image);
        out_images.push_back(light_vsm.device_light_depth_image);
    }
    return out_images;
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> VSMContext::get_required_descriptor_pool_size_and_sets() {
    return {
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2*lights_vsm.size()},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2*lights_vsm.size()}},
            static_cast<uint32_t>(2*lights_vsm.size())};
}

void VSMContext::create_image_views() {
    for (auto& light_vsm : lights_vsm) {
        vkDestroyImageView(device, light_vsm.device_vsm_depth_image_views[0], nullptr);
        vkDestroyImageView(device, light_vsm.device_vsm_depth_image_views[1], nullptr);
        vkDestroyImageView(device, light_vsm.device_light_depth_image_view, nullptr);

        VkImageViewCreateInfo image_view_create_info = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                nullptr,
                0,
                light_vsm.device_vsm_depth_image,
                VK_IMAGE_VIEW_TYPE_2D,
                VK_FORMAT_R32G32_SFLOAT,
                {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
        };
        check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &light_vsm.device_vsm_depth_image_views[0]), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
        image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1, 1 };
        check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &light_vsm.device_vsm_depth_image_views[1]), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);

        image_view_create_info.image = light_vsm.device_light_depth_image;
        image_view_create_info.format = VK_FORMAT_D32_SFLOAT;
        image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &light_vsm.device_light_depth_image_view), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
    }
}

void VSMContext::create_framebuffers() {
    // We need to create one framebuffer for each light
    for (auto& light_vsm : lights_vsm) {
        vkDestroyFramebuffer(device, light_vsm.framebuffer, nullptr);
        std::array<VkImageView,2> attachments {light_vsm.device_light_depth_image_view, light_vsm.device_vsm_depth_image_views[0] };
        VkFramebufferCreateInfo framebuffer_create_info = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                shadow_map_render_pass,
                attachments.size(),
                attachments.data(),
                light_vsm.depth_image_res.width,
                light_vsm.depth_image_res.height,
                1
        };
        check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &light_vsm.framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
    }
}

void VSMContext::init_resources() {
    create_image_views();
    create_framebuffers();
}

void VSMContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool) {
    // We need to create 2*images sets with the same layouts
    std::vector<VkDescriptorSetLayout> layouts_of_sets(2*lights_vsm.size(), vsm_descriptor_set_layout);
    vsm_descriptor_sets.resize(2*lights_vsm.size());
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(layouts_of_sets.size()),
            layouts_of_sets.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, vsm_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    // First 4 writes of write_descriptor_set
    std::vector<VkDescriptorImageInfo> descriptor_image_infos;
    descriptor_image_infos.reserve(4*lights_vsm.size());

    for (const auto& light_vsm : lights_vsm) {
        // First two are for the first pass of the blur
        descriptor_image_infos.push_back({
                device_render_target_sampler,
                light_vsm.device_vsm_depth_image_views[0],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        descriptor_image_infos.push_back({
                device_render_target_sampler,
                light_vsm.device_vsm_depth_image_views[1],
                VK_IMAGE_LAYOUT_GENERAL
        });

        // Second two are for the second pass of the blur
        descriptor_image_infos.push_back({
                device_render_target_sampler,
                light_vsm.device_vsm_depth_image_views[1],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        descriptor_image_infos.push_back({
                device_render_target_sampler,
                light_vsm.device_vsm_depth_image_views[0],
                VK_IMAGE_LAYOUT_GENERAL
        });
    }

    std::vector<VkWriteDescriptorSet> write_descriptor_set(2*vsm_descriptor_sets.size());
    for (uint32_t i=0; i<write_descriptor_set.size(); i++) {
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
        ++i;
        write_descriptor_set[i] = {
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
                                            VkDescriptorSet light_data_set, const std::vector<vk_object_render_info> &objects) {
    std::array<VkClearValue,2> clear_values;
    clear_values[0].depthStencil = {1.0f, 0};
    clear_values[1].color = {-40.0f, 1600.0f, 1.0f, 1.0f};

    for (int i=0; i < lights_vsm.size(); i++) {
        // We first render the shadowmap
        VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            shadow_map_render_pass,
            lights_vsm[i].framebuffer,
            {{0,0},{lights_vsm[i].depth_image_res}},
            clear_values.size(),
            clear_values.data()
        };
        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline);

        VkViewport viewport = {
                0.0f,
                0.0f,
                static_cast<float>(lights_vsm[i].depth_image_res.width),
                static_cast<float>(lights_vsm[i].depth_image_res.height),
                0.0f,
                1.0f
        };
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor = {
                {0,0},
                lights_vsm[i].depth_image_res
        };
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        vkCmdPushConstants(command_buffer, shadow_map_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &lights_vsm[i].ssbo_index);

        for (uint32_t j=0; j<object_data_sets.size(); j++) {
            std::array<VkDescriptorSet,2> sets_to_bind {object_data_sets[j], light_data_set};
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout, 0, sets_to_bind.size(), sets_to_bind.data(), 0, nullptr);

            vkCmdBindVertexBuffers(command_buffer, 0, 1, &objects[j].data_buffer, &objects[j].mesh_data_offset);
            vkCmdBindIndexBuffer(command_buffer, objects[j].data_buffer, objects[j].index_data_offset, objects[j].model.get_last_copy_index_type());
            vkCmdDrawIndexed(command_buffer, objects[j].model.get_last_copy_indices(), 1, 0, 0, 0);
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
                lights_vsm[i].device_vsm_depth_image,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 }
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipelines[0]);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_pipeline_layout, 0, 1, &vsm_descriptor_sets[2*i+0], 0, nullptr);
        vkCmdDispatch(command_buffer, std::ceil(lights_vsm[i].depth_image_res.width/32.0f), std::ceil(lights_vsm[i].depth_image_res.height/32.0f), 1);

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
                lights_vsm[i].device_vsm_depth_image,
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
                lights_vsm[i].device_vsm_depth_image,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 }
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, image_memory_barriers.data());

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipelines[1]);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_pipeline_layout, 0, 1, &vsm_descriptor_sets[2*i+1], 0, nullptr);
        vkCmdDispatch(command_buffer, std::ceil(lights_vsm[i].depth_image_res.width/32.0f), std::ceil(lights_vsm[i].depth_image_res.height/32.0f), 1);

        image_memory_barriers[0] = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                lights_vsm[i].device_vsm_depth_image,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 }
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());
    }
}

VkImageView VSMContext::get_image_view(int index) {
    return lights_vsm.at(index).device_vsm_depth_image_views[0];
}