#include "graphics_module_vulkan_app.h"
#include <vector>
#include <string>
#include "smaa/smaa_context.h"
#include "vsm/vsm_context.h"
#include "vulkan_helper.h"
#include <unordered_map>
#include <iostream>

GraphicsModuleVulkanApp::GraphicsModuleVulkanApp(const std::string &application_name, std::vector<const char *> &desired_instance_level_extensions,
                                                 VkExtent2D window_size, const std::vector<const char *> &desired_device_level_extensions,
                                                 const VkPhysicalDeviceFeatures &required_physical_device_features, VkBool32 surface_support, EngineOptions options) :
                         BaseVulkanApp(application_name,
                                       desired_instance_level_extensions,
                                       window_size,
                                       desired_device_level_extensions,
                                       required_physical_device_features,
                                       surface_support),
                         smaa_context(device, window_size),
                         vsm_context(device, window_size) {
    VkExtent3D screen_extent = {window_size.width, window_size.height, 1};

    std::vector<VkBuffer> device_buffers_to_allocate;
    std::vector<VkImage> device_images_to_allocate;

    // We create some attachments useful during rendering
    create_image(device_depth_image, VK_FORMAT_D32_SFLOAT, screen_extent, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    device_images_to_allocate.push_back(device_depth_image);

    create_image(device_render_target, VK_FORMAT_R32G32B32A32_SFLOAT, screen_extent, 2, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    device_images_to_allocate.push_back(device_render_target);

    // We request information about the images and buffer we need from smaa_context and store them in a vector
    auto smaa_array_images = smaa_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), smaa_array_images.begin(), smaa_array_images.end());
    device_buffers_to_allocate.push_back(smaa_context.get_device_buffer());

    // We request information about the images we need from vsm_context and store them in a vector
    device_images_to_allocate.push_back(vsm_context.get_device_image());

    // We then allocate all needed images and buffers in a single allocation
    allocate_and_bind_to_memory(device_attachments_memory, device_buffers_to_allocate, device_images_to_allocate, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // We then upload the images to memory
    smaa_context.upload_resource_images_to_device_memory("resources//textures//AreaTexDX10.R8G8", "resources//textures//SearchTex.R8",
                                                         physical_device_memory_properties, command_pool, command_buffers[0], queue);

    // For vsm we need to create the image_views
    vsm_context.create_image_views();

    create_image_view(device_depth_image_view, device_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1);
    create_image_view(device_render_target_image_views[0], device_render_target, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
    create_image_view(device_render_target_image_views[1], device_render_target, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);

    // Todo: move descriptor things in another method
    // Then we need to allocate a descriptor pool that can accomodate all sets
    std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> sets_elements_required;
    vulkan_helper::insert_or_sum(sets_elements_required, smaa_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, vsm_context.get_required_descriptor_pool_size_and_sets());

    for (auto& set_element_required : sets_elements_required.first) {
        std::cout << "Key:" << set_element_required.first << " Value:" << set_element_required.second << std::endl;
    }
}

GraphicsModuleVulkanApp::~GraphicsModuleVulkanApp() {
    vkDestroyImageView(device, device_depth_image_view, nullptr);
    vkDestroyImage(device, device_depth_image, nullptr);
    for (auto &device_render_target_image_view : device_render_target_image_views) {
        vkDestroyImageView(device, device_render_target_image_view, nullptr);
    }
    vkDestroyImage(device, device_render_target, nullptr);
    vkFreeMemory(device, device_attachments_memory, nullptr);

    // Uniform related things freed
    vkUnmapMemory(device, host_model_uniform_memory);
    vkDestroyBuffer(device, host_model_uniform_buffer, nullptr);
    vkFreeMemory(device, host_model_uniform_memory, nullptr);

    // Model related things freed
    for (auto& device_model_image_views : device_model_images_views) {
        for (auto & device_model_image_view : device_model_image_views) {
            vkDestroyImageView(device, device_model_image_view, nullptr);
        }
    }
    for (auto& device_model_image : device_model_images) {
        vkDestroyImage(device, device_model_image, nullptr);
    }
    vkDestroyBuffer(device, device_mesh_data_buffer, nullptr);
    vkFreeMemory(device, device_model_data_memory, nullptr);
}

void GraphicsModuleVulkanApp::load_3d_objects(std::vector<std::string> model_path, uint32_t object_matrix_size) {
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    std::vector<tinygltf::Model> models(model_path.size());

    // Get the total size of the models to allocate a buffer for it and copy them to host memory
    uint64_t models_total_size = 0;
    for (int i=0; i < model_path.size(); i++) {
        vulkan_helper::model_data_info model_data;
		check_error(!loader.LoadBinaryFromFile(&models[i], &err, &warn, model_path[i]), vulkan_helper::Error::MODEL_LOADING_FAILED);
        vulkan_helper::copy_gltf_contents(models[i],
                                          vulkan_helper::v_model_attributes::V_ALL,
                                          true,
                                          true,
                                          vulkan_helper::t_model_attributes::T_ALL,
                                          nullptr, model_data);
        models_total_size += vulkan_helper::get_model_data_total_size(model_data);
    }

    VkBuffer host_model_data_buffer = VK_NULL_HANDLE;
    create_buffer(host_model_data_buffer, models_total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkDeviceMemory host_model_data_memory = VK_NULL_HANDLE;
    allocate_and_bind_to_memory(host_model_data_memory, {host_model_data_buffer}, {}, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void *model_host_data_pointer;
    check_error(vkMapMemory(device, host_model_data_memory, 0, VK_WHOLE_SIZE, 0, &model_host_data_pointer), vulkan_helper::Error::POINTER_REQUEST_FOR_HOST_MEMORY_FAILED);

    // After getting a pointer for the host memory, we iterate once again in the models to copy them to memory and store some information about them
    uint64_t offset = 0;
    uint64_t mesh_and_index_data_size = 0;
    std::vector<vulkan_helper::model_data_info> model_data(models.size());
    for (int i=0; i < models.size(); i++) {
        vulkan_helper::copy_gltf_contents(models[i],
                                          vulkan_helper::v_model_attributes::V_ALL,
                                          true,
                                          true,
                                          vulkan_helper::t_model_attributes::T_ALL,
                                          static_cast<uint8_t*>(model_host_data_pointer) + offset, model_data[i]);
        offset += vulkan_helper::get_model_data_total_size(model_data[i]);
        mesh_and_index_data_size += vulkan_helper::get_model_mesh_and_index_data_size(model_data[i]);
    }
    vkUnmapMemory(device, host_model_data_memory);

    // After creating memory and buffer for the model data, we need to create a uniform buffer and their memory
    uint64_t uniform_size = vulkan_helper::get_aligned_memory_size(object_matrix_size, physical_device_properties.limits.minUniformBufferOffsetAlignment)*models.size();
    create_buffer(host_model_uniform_buffer, uniform_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    allocate_and_bind_to_memory(host_model_uniform_memory, {host_model_uniform_buffer}, {}, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(device, host_model_uniform_memory, 0, VK_WHOLE_SIZE, 0, &host_model_uniform_buffer_ptr);

    // After copying the data to host memory, we create a device buffer to hold the mesh and index data
    create_buffer(device_mesh_data_buffer, mesh_and_index_data_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // We must also create images for the textures of every object, after destroying the precedent images
    for(auto &device_model_image : device_model_images) {
        vkDestroyImage(device, device_model_image, nullptr);
    }
    device_model_images.clear();
    for (int i=0; i<model_data.size(); i++) {
        if (vulkan_helper::get_model_texture_size(model_data[i])) {
            device_model_images.push_back(VK_NULL_HANDLE);
            create_image(device_model_images.back(), VK_FORMAT_R8G8B8A8_UNORM, model_data[i].image_size, model_data[i].image_layers,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);
        }
    }
    allocate_and_bind_to_memory(device_model_data_memory, {device_mesh_data_buffer}, device_model_images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // After the buffer and the images have been allocated and binded, we process to create the image views
    for (auto& device_model_image_views : device_model_images_views) {
        for (auto & device_model_image_view : device_model_image_views) {
            vkDestroyImageView(device, device_model_image_view, nullptr);
        }
        device_model_image_views.clear();
    }
    device_model_images_views.clear();

    // Here when creating the images views we say that the first image view of an image is of type srgb and the other ones are unorm
    device_model_images_views.resize(device_model_images.size());
    for (uint32_t i=0; i<device_model_images.size(); i++) {
        device_model_images_views[i].resize(model_data[i].image_layers);
        for (uint32_t j=0; j<device_model_images_views.size(); j++) {
            VkFormat image_format = (j == 0) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            create_image_view(device_model_images_views[i][j], device_model_images[i], image_format, VK_IMAGE_ASPECT_COLOR_BIT, j, 1);
        }
    }

    // After setting the required memory we register the command buffer to upload the data
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        nullptr
    };
    vkBeginCommandBuffer(command_buffers[0], &command_buffer_begin_info);

    // We calculate the offset for the mesh data both in the host and device buffers and register the copy
    uint64_t src_offset = 0;
    uint64_t dst_offset = 0;
    std::vector<VkBufferCopy> buffer_copies(models.size());
    for (int i=0; i<buffer_copies.size(); i++) {
        buffer_copies[i].srcOffset = src_offset;
        src_offset += vulkan_helper::get_model_data_total_size(model_data[i]);
        buffer_copies[i].dstOffset = dst_offset;
        dst_offset += vulkan_helper::get_model_mesh_and_index_data_size(model_data[i]);
        buffer_copies[i].size = vulkan_helper::get_model_mesh_and_index_data_size(model_data[i]);
    }
    vkCmdCopyBuffer(command_buffers[0], host_model_data_buffer, device_mesh_data_buffer, buffer_copies.size(), buffer_copies.data());

    // We need to transition the device images to DST_OPTIMAL
    std::vector<VkImageMemoryBarrier> image_memory_barriers(device_model_images.size());
    for (int i=0; i<image_memory_barriers.size(); i++) {
        image_memory_barriers[i] = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            device_model_images[i],
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
        };
    }
    vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

    // We perform the copy from the host buffer to the device images
    uint64_t image_data_offset = 0;
    for (int i=0; i<device_model_images.size(); i++) {
        image_data_offset += vulkan_helper::get_model_mesh_and_index_data_size(model_data[i]);
        VkBufferImageCopy buffer_image_copy = {
            image_data_offset,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,4},
            {0,0,0},
            model_data[i].image_size
        };
        image_data_offset += vulkan_helper::get_model_texture_size(model_data[i]);
        vkCmdCopyBufferToImage(command_buffers[0], host_model_data_buffer, device_model_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
    }

    // After the copy we need to transition the images to be shader ready
    for (int i=0; i<image_memory_barriers.size(); i++) {
        image_memory_barriers[i] = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            device_model_images[i],
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
        };
    }
    vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());
    vkEndCommandBuffer(command_buffers[0]);

    // After registering the command we create a fence and submit the command_buffer
    VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,nullptr,0 };
    VkFence fence;
    vkCreateFence(device, &fence_create_info, nullptr, &fence);

    submit_command_buffers({command_buffers[0]}, VK_PIPELINE_STAGE_TRANSFER_BIT, {}, {}, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, 20000000);

    // After the copy has finished we do cleanup
    vkResetCommandPool(device, command_pool, 0);
    vkDestroyFence(device, fence, nullptr);
    vkDestroyBuffer(device, host_model_data_buffer, nullptr);
    vkFreeMemory(device, host_model_data_memory, nullptr);
}

void GraphicsModuleVulkanApp::create_buffer(VkBuffer &buffer, uint64_t size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo buffer_create_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        size,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    vkDestroyBuffer(device, buffer, nullptr);
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer), vulkan_helper::Error::BUFFER_CREATION_FAILED);
}

void GraphicsModuleVulkanApp::create_image(VkImage &image, VkFormat format, VkExtent3D image_size, uint32_t layers, VkImageUsageFlags usage_flags, VkImageCreateFlags create_flags) {
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            create_flags,
            VK_IMAGE_TYPE_2D,
            format,
            image_size,
            1,
            layers,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            usage_flags,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED
    };
    vkDestroyImage(device, image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
}

void GraphicsModuleVulkanApp::create_image_view(VkImageView &image_view, VkImage image, VkFormat image_format, VkImageAspectFlags aspect_mask, uint32_t start_layer, uint32_t layer_count) {
    VkImageViewCreateInfo image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            image_format,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            {aspect_mask, 0, VK_REMAINING_MIP_LEVELS, start_layer, layer_count}
    };
    vkDestroyImageView(device, image_view, nullptr);
    check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &image_view), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
}

void GraphicsModuleVulkanApp::allocate_and_bind_to_memory(VkDeviceMemory &memory, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VkMemoryPropertyFlags flags) {
    std::vector<VkMemoryRequirements> memory_requirements(buffers.size() + images.size());
    for (int i=0; i<buffers.size(); i++) {
        vkGetBufferMemoryRequirements(device, buffers[i], &memory_requirements[i]);
    }
    for (int i=0; i<images.size(); i++) {
        vkGetImageMemoryRequirements(device, images[i], &memory_requirements[i+buffers.size()]);
    }

    uint64_t allocation_size = 0;
    for (int i = 0; i < memory_requirements.size(); i++) {
        allocation_size += memory_requirements[i].size;
        if ((i+1) < memory_requirements.size()) {
            allocation_size = vulkan_helper::get_aligned_memory_size(allocation_size, memory_requirements.at(i+1).alignment);
        }
    }

    VkMemoryAllocateInfo memory_allocate_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        allocation_size,
        vulkan_helper::select_memory_index(physical_device_memory_properties, memory_requirements.back(), flags)
    };
    vkFreeMemory(device, memory, nullptr);
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &memory), vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    auto sum_offsets = [&](int idx) {
        int sum = 0;
        for (int i = 0; i < idx; i++) {
            sum += memory_requirements[i].size;
            if ((i+1) < memory_requirements.size()) {
                sum = vulkan_helper::get_aligned_memory_size(sum, memory_requirements.at(i+1).alignment);
            }
        }
        return sum;
    };

    for (int i=0; i<buffers.size(); i++) {
        check_error(vkBindBufferMemory(device, buffers[i], memory, sum_offsets(i)), vulkan_helper::Error::BIND_BUFFER_MEMORY_FAILED);
    }
    for (int i=0; i<images.size(); i++) {
        check_error(vkBindImageMemory(device, images[i], memory, sum_offsets(i+buffers.size())), vulkan_helper::Error::BIND_IMAGE_MEMORY_FAILED);
    }
}

void GraphicsModuleVulkanApp::submit_command_buffers(std::vector<VkCommandBuffer> command_buffers, VkPipelineStageFlags pipeline_stage_flags,
                                                     std::vector<VkSemaphore> wait_semaphores, std::vector<VkSemaphore> signal_semaphores, VkFence fence) {
    VkSubmitInfo submit_info = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            static_cast<uint32_t>(wait_semaphores.size()),
            wait_semaphores.data(),
            &pipeline_stage_flags,
            static_cast<uint32_t>(command_buffers.size()),
            command_buffers.data(),
            static_cast<uint32_t>(signal_semaphores.size()),
            signal_semaphores.data(),
    };
    check_error(vkQueueSubmit(queue, 1, &submit_info, fence), vulkan_helper::Error::QUEUE_SUBMIT_FAILED);
}
