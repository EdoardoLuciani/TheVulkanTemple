#include "vsm_context.h"
#include "../vulkan_helper.h"

VSMContext::VSMContext(VkDevice device, VkExtent2D screen_res) {
    this->device = device;

    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32G32_SFLOAT,
            {screen_res.width, screen_res.height, 1},
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
}

VSMContext::~VSMContext() {
    vkDestroySampler(device, device_shadow_map_sampler, nullptr);
    vkDestroyImageView(device, device_vsm_depth_image_views[0], nullptr);
    vkDestroyImageView(device, device_vsm_depth_image_views[1], nullptr);
    vkDestroyImage(device, device_vsm_depth_image, nullptr);
}

VkImage VSMContext::get_device_image() {
    return device_vsm_depth_image;
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
}