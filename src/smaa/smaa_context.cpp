#include "smaa_context.h"
#include <string>
#include "../volk.h"
#include "../vulkan_helper.h"

SmaaContext::SmaaContext(VkDevice device) {
    this->device = device;

    // Sampler we will use with the smaa images
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
            1,
            VK_FALSE,
            0.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0f,
            1.0f,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &device_render_target_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    std::array<VkSampler, 3> immutable_samplers;
    immutable_samplers.fill(device_render_target_sampler);

    // We also need to create the layouts for the descriptors
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            immutable_samplers.data()
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &descriptor_set_layout_binding
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[0]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Layout for the second descriptor for the second smaa pipeline
    descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            3,
            VK_SHADER_STAGE_VERTEX_BIT |VK_SHADER_STAGE_FRAGMENT_BIT,
            immutable_samplers.data()
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[1]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Layout for the third descriptor for the third smaa pipeline, same as the first
    descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            VK_SHADER_STAGE_VERTEX_BIT |VK_SHADER_STAGE_FRAGMENT_BIT,
            immutable_samplers.data()
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[2]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Smaa edge renderpass
    std::array<VkAttachmentDescription, 2> attachment_description {{
    {
            0,
            VK_FORMAT_S8_UINT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    },
    {
            0,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }}};

    std::array<VkAttachmentReference, 2> attachment_reference {{
            { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
            { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    }};
    VkSubpassDescription subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &attachment_reference[1],
            nullptr,
            &attachment_reference[0],
            0,
            nullptr
    };

    VkRenderPassCreateInfo render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            attachment_description.size(),
            attachment_description.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_passes[0]);
}

SmaaContext::~SmaaContext() {
    for (auto& smaa_descriptor_set_layout : smaa_descriptor_sets_layout) {
        vkDestroyDescriptorSetLayout(device, smaa_descriptor_set_layout, nullptr);
    }
    vkDestroySampler(device, device_render_target_sampler, nullptr);

    for (auto& render_pass : render_passes) {
        vkDestroyRenderPass(device, render_pass, nullptr);
    }

    vkDestroyImageView(device, device_smaa_area_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_search_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_stencil_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_data_edge_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_data_weight_image_view, nullptr);

    vkDestroyImage(device, device_smaa_area_image, nullptr);
    vkDestroyImage(device, device_smaa_search_image, nullptr);
    vkDestroyImage(device, device_smaa_stencil_image, nullptr);
    vkDestroyImage(device, device_smaa_data_image, nullptr);

    for (auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for(auto& pipeline_layout : smaa_pipelines_layout) {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    }
    for(auto& pipeline : smaa_pipelines) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
}

std::array<VkImage, 4> SmaaContext::get_device_images() {
    return std::array<VkImage, 4>{device_smaa_area_image, device_smaa_search_image, device_smaa_stencil_image, device_smaa_data_image};
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> SmaaContext::get_required_descriptor_pool_size_and_sets() {
    return {
            {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7}},
            3
    };
}

void SmaaContext::create_resources(VkExtent2D screen_res, std::string shader_dir_path) {
    screen_extent = {screen_res.width, screen_res.height, 1};
    create_edge_pipeline(shader_dir_path);

    // Then we create the device images for the resources
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            smaa_area_image_format,
            smaa_area_image_extent,
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED
    };
    vkDestroyImage(device, device_smaa_area_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_area_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    image_create_info.format = VK_FORMAT_R8_UNORM;
    image_create_info.extent = smaa_search_image_extent;
    vkDestroyImage(device, device_smaa_search_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_search_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // We create the images required for the smaa, first the stencil image
    image_create_info.format = VK_FORMAT_S8_UINT;
    image_create_info.extent = screen_extent;
    image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vkDestroyImage(device, device_smaa_stencil_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_stencil_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // Then the data_image which is gonna hold the edge and weight tex during rendering
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.arrayLayers = 2;
    image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkDestroyImage(device, device_smaa_data_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_data_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

}

void SmaaContext::create_edge_pipeline(std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_edge.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_edge.frag.spv", shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
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
            static_cast<float>(screen_extent.width),
            static_cast<float>(screen_extent.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {screen_extent.width, screen_extent.height}
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
            VK_TRUE,
            {VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_GREATER, 0xff, 0xff, 1},
            {VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_GREATER, 0xff, 0xff, 1},
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

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &smaa_descriptor_sets_layout[0],
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, smaa_pipelines_layout[0], nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_pipelines_layout[0]);

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
            nullptr,
            smaa_pipelines_layout[0],
            render_passes[0],
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, smaa_pipelines[0], nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_pipelines[0]);
    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void SmaaContext::create_image_views() {
    VkImageViewCreateInfo image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            device_smaa_area_image,
            VK_IMAGE_VIEW_TYPE_2D,
            smaa_area_image_format,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
    };
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_area_image_view);

    image_view_create_info.image = device_smaa_search_image;
    image_view_create_info.format = smaa_search_image_format;
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_search_image_view);

    image_view_create_info.image = device_smaa_stencil_image;
    image_view_create_info.format = VK_FORMAT_S8_UINT;
    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0 , 1 };
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_stencil_image_view);

    image_view_create_info.image = device_smaa_data_image;
    image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_data_edge_image_view);

    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1, 1 };
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_data_weight_image_view);
}

void SmaaContext::create_framebuffers() {
    std::array<VkImageView, 2> attachments = {
            device_smaa_stencil_image_view,
            device_smaa_data_edge_image_view
    };
    VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            render_passes[0],
            attachments.size(),
            attachments.data(),
            screen_extent.width,
            screen_extent.height,
            screen_extent.depth
    };
    vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[0]);
}

void SmaaContext::init_resources(std::string support_images_dir, const VkPhysicalDeviceMemoryProperties &memory_properties,
                                 VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue) {
    // We know that the images have already been allocated so we create the views and the sampler
    create_image_views();
    create_framebuffers();

    // We need to get the smaa resource images from disk and upload them to VRAM
    uint64_t area_tex_size, search_tex_size;

    vulkan_helper::get_binary_file_content(support_images_dir + "//AreaTexDX10.R8G8", area_tex_size, nullptr);
    vulkan_helper::get_binary_file_content(support_images_dir + "//SearchTex.R8", search_tex_size, nullptr);

    // First we create the buffer and allocate it
    VkBufferCreateInfo buffer_create_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            area_tex_size + search_tex_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr
    };
    VkBuffer host_transition_buffer;
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &host_transition_buffer), vulkan_helper::Error::BUFFER_CREATION_FAILED);
    VkMemoryRequirements host_transition_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(device, host_transition_buffer, &host_transition_buffer_memory_requirements);

    VkDeviceMemory host_transition_memory;
    VkMemoryAllocateInfo memory_allocate_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            host_transition_buffer_memory_requirements.size,
            vulkan_helper::select_memory_index(memory_properties, host_transition_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &host_transition_memory), vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);
    check_error(vkBindBufferMemory(device, host_transition_buffer, host_transition_memory, 0), vulkan_helper::Error::BIND_BUFFER_MEMORY_FAILED);

    // We map the memory and copy the data
    void *dst_ptr;
    check_error(vkMapMemory(device, host_transition_memory, 0, VK_WHOLE_SIZE, 0, &dst_ptr), vulkan_helper::Error::POINTER_REQUEST_FOR_HOST_MEMORY_FAILED);
    vulkan_helper::get_binary_file_content(support_images_dir + "//AreaTexDX10.R8G8", area_tex_size, dst_ptr);
    vulkan_helper::get_binary_file_content(support_images_dir + "//SearchTex.R8", search_tex_size, static_cast<uint8_t*>(dst_ptr) + area_tex_size);
    vkUnmapMemory(device, host_transition_memory);

    // Then we record the command buffer to submit the command
    VkCommandBufferBeginInfo command_buffer_begin_info = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,nullptr
    };
    vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    std::array<VkImageMemoryBarrier, 2> image_memory_barriers;
    image_memory_barriers[0] = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        device_smaa_area_image,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    image_memory_barriers[1] = image_memory_barriers[0];
    image_memory_barriers[1].image = device_smaa_search_image;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

    // We perform the copy from the host buffer to the device images
    VkBufferImageCopy buffer_image_copy = {
            0,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
            {0,0,0},
            smaa_area_image_extent
    };
    vkCmdCopyBufferToImage(command_buffer, host_transition_buffer, device_smaa_area_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
    buffer_image_copy = {
            area_tex_size,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
            {0,0,0},
            smaa_search_image_extent
    };
    vkCmdCopyBufferToImage(command_buffer, host_transition_buffer, device_smaa_search_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);


    // After the copy we need to transition the images to be shader ready
    image_memory_barriers[0] = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            device_smaa_area_image,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    image_memory_barriers[1] = image_memory_barriers[0];
    image_memory_barriers[1].image = device_smaa_search_image;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());
    vkEndCommandBuffer(command_buffer);

    VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,nullptr,0 };
    VkFence fence;
    vkCreateFence(device, &fence_create_info, nullptr, &fence);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            0,
            nullptr,
            &flags,
            0,
            nullptr,
            0,
            nullptr,
    };
    check_error(vkQueueSubmit(queue, 1, &submit_info, fence), vulkan_helper::Error::QUEUE_SUBMIT_FAILED);
    vkWaitForFences(device, 1, &fence, VK_TRUE, 20000000);

    vkDestroyFence(device, fence, nullptr);
    vkResetCommandPool(device, command_pool, 0);
    vkDestroyBuffer(device, host_transition_buffer, nullptr);
    vkFreeMemory(device, host_transition_memory, nullptr);
}

void SmaaContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView depth_image_view) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(smaa_descriptor_sets_layout.size()),
            smaa_descriptor_sets_layout.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, smaa_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    std::array<VkDescriptorImageInfo, 2> smaa_edge_descriptor_images_info {{
            { device_render_target_sampler, input_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, depth_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    }};

    std::array<VkDescriptorImageInfo, 3> smaa_weight_descriptor_images_info {{
            { device_render_target_sampler, device_smaa_data_edge_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, device_smaa_area_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, device_smaa_search_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    }};

    std::array<VkDescriptorImageInfo, 2> smaa_blend_descriptor_images_info {{
            { device_render_target_sampler, input_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, device_smaa_data_weight_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    }};

    std::array<VkWriteDescriptorSet,3> write_descriptor_set {{{
        // First write is for the smaa edge descriptor
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[0],
        0,
        0,
        smaa_edge_descriptor_images_info.size(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        smaa_edge_descriptor_images_info.data(),
        nullptr,
        nullptr
        },
            // Second writes are for the smaa weight descriptor
        {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[1],
        0,
        0,
        smaa_weight_descriptor_images_info.size(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        smaa_weight_descriptor_images_info.data(),
        nullptr,
        nullptr
        },
            // Third write is for the smaa blend descriptor
        {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[2],
        0,
        0,
        smaa_blend_descriptor_images_info.size(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        smaa_blend_descriptor_images_info.data(),
        nullptr,
        nullptr
        }
    }};
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}

void SmaaContext::record_into_command_buffer(VkCommandBuffer command_buffer) {
    std::array<VkClearValue , 2> clear_colors;
    clear_colors[0].depthStencil = { 1.0f, 0 };
    clear_colors[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };

    VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            render_passes[0],
            framebuffers[0],
            {{0,0},{screen_extent.width, screen_extent.height}},
            clear_colors.size(),
            clear_colors.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines[0]);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines_layout[0], 0, 1, &smaa_descriptor_sets[0], 0, nullptr);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    // Smaa edge renderpass end
    vkCmdEndRenderPass(command_buffer);
}
