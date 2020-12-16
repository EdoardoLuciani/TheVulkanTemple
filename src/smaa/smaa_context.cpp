#include "smaa_context.h"
#include <string>
#include "../volk.h"
#include "../vulkan_helper.h"

SmaaContext::SmaaContext(VkDevice device, VkExtent2D screen_res) {
    this->device = device;

    // Then we create the device images for the resources
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8_UNORM,
             area_image_size,
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
    image_create_info.extent = search_image_size;
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_search_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // We create the images required for the smaa, first the stencil image
    image_create_info.format = VK_FORMAT_S8_UINT;
    image_create_info.extent = { screen_res.width, screen_res.height, 1 };
    image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_stencil_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // Then the data_image which is gonna hold the edge and bled tex during rendering
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.arrayLayers = 2;
    image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_data_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
}

SmaaContext::~SmaaContext() {
    vkDestroyImage(device, device_smaa_area_image, nullptr);
    vkDestroyImage(device, device_smaa_search_image, nullptr);
    vkDestroyImage(device, device_smaa_stencil_image, nullptr);
    vkDestroyImage(device, device_smaa_data_image, nullptr);
}

std::array<VkImage, 4> SmaaContext::get_device_images() {
    return {device_smaa_area_image, device_smaa_search_image, device_smaa_stencil_image, device_smaa_data_image};
}

void SmaaContext::upload_resource_images_to_device_memory(std::string area_tex_path, std::string search_tex_path, const VkPhysicalDeviceMemoryProperties &memory_properties,
                                                          VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue) {
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
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
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
            area_image_size
    };
    vkCmdCopyBufferToImage(command_buffer, host_transition_buffer, device_smaa_area_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
    buffer_image_copy = {
            area_tex_size,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
            {0,0,0},
            search_image_size
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
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
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
