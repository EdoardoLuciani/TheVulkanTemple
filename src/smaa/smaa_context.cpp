#include "smaa_context.h"
#include <string>
#include "../volk.h"
#include "../vulkan_helper.h"

SmaaContext::SmaaContext(VkDevice device, VkExtent2D screen_res) {
    this->device = device;
    screen_extent = {screen_res.width, screen_res.height, 1};

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
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_area_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    image_create_info.format = VK_FORMAT_R8_UNORM;
    image_create_info.extent = smaa_search_image_extent;
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_search_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // We create the images required for the smaa, first the stencil image
    image_create_info.format = VK_FORMAT_S8_UINT;
    image_create_info.extent = screen_extent;
    image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_stencil_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // Then the data_image which is gonna hold the edge and weight tex during rendering
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.arrayLayers = 2;
    image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_data_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // We need to create a device buffer that is gonna hold SMAA_RT_METRICS of size 16b
    VkBufferCreateInfo buffer_create_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            16,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr
    };
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &device_smaa_rt_metrics_buffer), vulkan_helper::Error::BUFFER_CREATION_FAILED);

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

    // We also need to create the layouts for the descriptors
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
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
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[1]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Layout for the third descriptor for the third smaa pipeline, same as the first
    descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[2]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Last layout is for the SMAA_RT_METRICS which contain the resolution
    descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[3]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);
}

SmaaContext::~SmaaContext() {
    for (auto& smaa_descriptor_set_layout : smaa_descriptor_sets_layout) {
        vkDestroyDescriptorSetLayout(device, smaa_descriptor_set_layout, nullptr);
    }
    vkDestroySampler(device, device_render_target_sampler, nullptr);
    vkDestroyBuffer(device, device_smaa_rt_metrics_buffer, nullptr);
    vkDestroyImageView(device, device_smaa_area_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_search_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_stencil_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_data_edge_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_data_weight_image_view, nullptr);

    vkDestroyImage(device, device_smaa_area_image, nullptr);
    vkDestroyImage(device, device_smaa_search_image, nullptr);
    vkDestroyImage(device, device_smaa_stencil_image, nullptr);
    vkDestroyImage(device, device_smaa_data_image, nullptr);
}

std::pair<VkBuffer, std::array<VkImage, 4>> SmaaContext::get_device_buffers_and_images() {
    return std::make_pair(device_smaa_rt_metrics_buffer,
                          std::array<VkImage, 4>{device_smaa_area_image, device_smaa_search_image, device_smaa_stencil_image, device_smaa_data_image});
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> SmaaContext::get_required_descriptor_pool_size_and_sets() {
    return {
            {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}},
            4
    };
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

void SmaaContext::init_resources(std::string area_tex_path, std::string search_tex_path, const VkPhysicalDeviceMemoryProperties &memory_properties,
                                 VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue) {
    // We know that the images have already been allocated so we create the views and the sampler
    create_image_views();

    // We need to get the smaa resource images from disk and upload them to VRAM
    uint64_t area_tex_size, search_tex_size;

    vulkan_helper::get_binary_file_content(area_tex_path, area_tex_size, nullptr);
    vulkan_helper::get_binary_file_content(search_tex_path, search_tex_size, nullptr);

    // First we create the buffer and allocate it
    VkBufferCreateInfo buffer_create_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            area_tex_size+search_tex_size,
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
    vulkan_helper::get_binary_file_content(area_tex_path,area_tex_size, dst_ptr);
    vulkan_helper::get_binary_file_content(search_tex_path,search_tex_size, static_cast<uint8_t*>(dst_ptr) + area_tex_size);
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

    vkCmdUpdateBuffer(command_buffer, device_smaa_rt_metrics_buffer, 0, sizeof(glm::vec4),
                      glm::value_ptr(glm::vec4(1.0f / screen_extent.width,
                                               1.0f / screen_extent.height,
                                               static_cast<float>(screen_extent.width),
                                               static_cast<float>(screen_extent.height))));

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

    // Also for the smaa_rt_metrics buffer
    VkMemoryBarrier memory_barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_UNIFORM_READ_BIT
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());
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

    VkDescriptorBufferInfo smaa_rt_metrics = {device_smaa_rt_metrics_buffer, 0, sizeof(glm::vec4)};

    std::array<VkWriteDescriptorSet,4> write_descriptor_set {{{
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
        },
            // Fourth write is for smaa_rt_metrics
        {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[3],
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        nullptr,
        &smaa_rt_metrics,
        nullptr
        }
    }};
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);

}
