#include "vsm_context.h"
#include "../vulkan_helper.h"
#include <unordered_map>
#include <utility>

VSMContext::VSMContext(VkDevice device, VkExtent2D depth_image_res) {
    this->device = device;
    this->depth_image_res = depth_image_res;

    // We create the images required for vsm
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32G32_SFLOAT,
            {depth_image_res.width, depth_image_res.height, 1},
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
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_vsm_depth_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // Creation of the buffer to hold the depth_image_size
    VkBufferCreateInfo buffer_create_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            sizeof(glm::vec4),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr
    };
    vkCreateBuffer(device, &buffer_create_info, nullptr, &device_vsm_extent_buffer);

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
    std::array<VkDescriptorSetLayoutBinding, 3> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
    };
    descriptor_set_layout_binding[2] = {
            2,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &vsm_descriptor_set_layout), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);
}

VSMContext::~VSMContext() {
    vkDestroyDescriptorSetLayout(device, vsm_descriptor_set_layout, nullptr);
    vkDestroySampler(device, device_shadow_map_sampler, nullptr);
    vkDestroySampler(device, device_max_aniso_linear_sampler, nullptr);
    vkDestroyImageView(device, device_vsm_depth_image_views[0], nullptr);
    vkDestroyImageView(device, device_vsm_depth_image_views[1], nullptr);
    vkDestroyImage(device, device_vsm_depth_image, nullptr);
}

std::pair<VkBuffer,VkImage> VSMContext::get_device_buffer_and_image() {
    return std::make_pair(device_vsm_extent_buffer, device_vsm_depth_image);
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> VSMContext::get_required_descriptor_pool_size_and_sets() {
    return {
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2} },
            2};
}

void VSMContext::create_image_views() {
    VkImageViewCreateInfo image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            device_vsm_depth_image,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R32G32_SFLOAT,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
    };
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_vsm_depth_image_views[0]);
    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1, 1 };
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_vsm_depth_image_views[1]);
}

void VSMContext::init_resources(VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue) {
    create_image_views();

    // we submit a command to set vsm_extent_buffer
    VkCommandBufferBeginInfo command_buffer_begin_info = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,nullptr
    };
    vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    vkCmdUpdateBuffer(command_buffer, device_vsm_extent_buffer, 0, sizeof(glm::vec4),
                      glm::value_ptr(glm::vec4(1.0f / depth_image_res.width,
                                               1.0f / depth_image_res.height,
                                               static_cast<float>(depth_image_res.width),
                                               static_cast<float>(depth_image_res.height))));
    // Also for the smaa_rt_metrics buffer
    VkMemoryBarrier memory_barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_UNIFORM_READ_BIT
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memory_barrier, 0, nullptr, 0, nullptr);
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
}

void VSMContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool) {
    // We need to create two sets with the same layouts
    std::array<VkDescriptorSetLayout, 3> layouts_of_sets = {vsm_descriptor_set_layout, vsm_descriptor_set_layout};
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            layouts_of_sets.size(),
            layouts_of_sets.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, vsm_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    // First two are for the first pass of the blur
    std::array<VkDescriptorImageInfo, 5> descriptor_image_infos;
    descriptor_image_infos[0] = {
            device_max_aniso_linear_sampler,
            device_vsm_depth_image_views[0],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    descriptor_image_infos[1] = {
            device_max_aniso_linear_sampler,
            device_vsm_depth_image_views[1],
            VK_IMAGE_LAYOUT_GENERAL
    };

    // Second two are for the second pass of the blur
    descriptor_image_infos[2] = {
            device_max_aniso_linear_sampler,
            device_vsm_depth_image_views[1],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    descriptor_image_infos[3] = {
            device_max_aniso_linear_sampler,
            device_vsm_depth_image_views[0],
            VK_IMAGE_LAYOUT_GENERAL
    };

    // descriptor_buffer_info is the same for both sets
    VkDescriptorBufferInfo descriptor_buffer_info = {
            device_vsm_extent_buffer,
            0,
            sizeof(glm::vec4)
    };

    std::array<VkWriteDescriptorSet, 6> write_descriptor_set;
    // First 4 writes of write_descriptor_set
    for (uint32_t i=0; i<2; i++) {
        write_descriptor_set[3*i] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            vsm_descriptor_sets[i],
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &descriptor_image_infos[2*i],
            nullptr,
            nullptr
        };
        write_descriptor_set[3*i+1] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            vsm_descriptor_sets[i],
            1,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &descriptor_image_infos[2*i+1],
            nullptr,
            nullptr
        };
        write_descriptor_set[3*i+2] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            vsm_descriptor_sets[i],
            2,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            nullptr,
            &descriptor_buffer_info,
            nullptr
        };
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}