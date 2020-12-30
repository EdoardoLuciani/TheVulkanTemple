#include "vsm_context.h"
#include "../vulkan_helper.h"
#include <unordered_map>
#include <utility>
#include <cstring>
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

    vkDestroyBuffer(device, device_vsm_extent_buffer, nullptr);
}

void VSMContext::create_resources(std::vector<VkExtent2D> depth_images_res, uint64_t min_uniform_offset_alignment) {
    this->depth_images_res = depth_images_res;
    this->min_uniform_offset_alignment = min_uniform_offset_alignment;

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

    // Creation of the buffer to hold the depth_image_sizes
    VkBufferCreateInfo buffer_create_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            vulkan_helper::get_aligned_memory_size(sizeof(glm::vec2), min_uniform_offset_alignment)*depth_images_res.size(),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr
    };
    vkCreateBuffer(device, &buffer_create_info, nullptr, &device_vsm_extent_buffer);
}

std::pair<VkBuffer,std::vector<VkImage>> VSMContext::get_device_buffer_and_images() {
    auto vec = device_vsm_depth_images;
    vec.insert(vec.begin(), device_light_depth_images.begin(), device_light_depth_images.end());
    return std::make_pair(device_vsm_extent_buffer, vec);
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> VSMContext::get_required_descriptor_pool_size_and_sets() {
    return {
        { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2*device_vsm_depth_images.size()},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2*device_vsm_depth_images.size()},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2*device_vsm_depth_images.size()}},
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

void VSMContext::init_resources(VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue) {
    create_image_views();
    create_framebuffers();

    // we submit a command to set vsm_extent_buffer
    VkCommandBufferBeginInfo command_buffer_begin_info = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,nullptr
    };
    vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

    for (uint32_t i=0; i<depth_images_res.size(); i++) {
        vkCmdUpdateBuffer(command_buffer, device_vsm_extent_buffer, i*vulkan_helper::get_aligned_memory_size(sizeof(glm::vec2), min_uniform_offset_alignment), sizeof(glm::vec2),
                          glm::value_ptr(glm::vec2(static_cast<float>(depth_images_res[i].width),static_cast<float>(depth_images_res[i].height))));
    }

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

    std::vector<VkDescriptorBufferInfo> descriptor_buffer_info(device_vsm_depth_images.size());
    for (int i=0; i<descriptor_buffer_info.size(); i++) {
        descriptor_buffer_info[i] = {
                device_vsm_extent_buffer,
                i*sizeof(glm::vec2),
                sizeof(glm::vec2)
        };
    }

    std::vector<VkWriteDescriptorSet> write_descriptor_set(3*vsm_descriptor_sets.size());
    for (int i=0; i<write_descriptor_set.size(); i++) {
        write_descriptor_set[i] = {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    vsm_descriptor_sets[i/3],
                    0,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    &descriptor_image_infos[i-i/3],
                    nullptr,
                    nullptr
        };
        //i++;
        write_descriptor_set[++i] = {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    vsm_descriptor_sets[i/3],
                    1,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    &descriptor_image_infos[i-i/3],
                    nullptr,
                    nullptr
        };
        //i++;
        write_descriptor_set[++i] = {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    vsm_descriptor_sets[i/3],
                    2,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    nullptr,
                    &descriptor_buffer_info[i/6],
                    nullptr
        };
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}

VkImageView VSMContext::get_image_view(int index) {
    return device_vsm_depth_image_views.at(index)[0];
}