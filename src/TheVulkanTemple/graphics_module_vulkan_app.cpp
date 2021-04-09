#include "graphics_module_vulkan_app.h"
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include "smaa/smaa_context.h"
#include "vsm/vsm_context.h"
#include "hbao/hbao_context.h"
#include "hdr_tonemap/hdr_tonemap_context.h"
#include "vulkan_helper.h"
#include <unordered_map>

GraphicsModuleVulkanApp::GraphicsModuleVulkanApp(const std::string &application_name,
                                                 std::vector<const char *> &desired_instance_level_extensions,
                                                 VkExtent2D window_size,
                                                 bool fullscreen,
                                                 const std::vector<const char *> &desired_device_level_extensions,
                                                 const VkPhysicalDeviceFeatures &required_physical_device_features,
                                                 VkBool32 surface_support,
                                                 EngineOptions options,
                                                 void* additional_structure) :
                         BaseVulkanApp(application_name,
                                       desired_instance_level_extensions,
                                       window_size,
                                       fullscreen,
                                       desired_device_level_extensions,
                                       required_physical_device_features,
                                       surface_support,
                                       additional_structure),
                         vsm_context(device),
                         smaa_context(device, VK_FORMAT_B10G11R11_UFLOAT_PACK32),
                         hbao_context(device, window_size, VK_FORMAT_D32_SFLOAT, "resources//shaders"),
                         hdr_tonemap_context(device, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R16_SFLOAT, swapchain_create_info.imageFormat) {

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
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &max_aniso_linear_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
    semaphores.resize(2);
    for (uint32_t i = 0; i < semaphores.size(); i++) {
        vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores[i]);
    }

    create_render_pass();
    create_sets_layouts();
}

void GraphicsModuleVulkanApp::create_render_pass() {
    std::array<VkAttachmentDescription, 3> attachment_descriptions {{
    {
                0,
                VK_FORMAT_D32_SFLOAT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        {
                0,
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                VK_SAMPLE_COUNT_1_BIT,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        },
        {
                0,
                VK_FORMAT_R16G16B16A16_SFLOAT,
                VK_SAMPLE_COUNT_1_BIT,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        }
    }};
    std::array<VkAttachmentReference,3> attachment_references {{
        { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    }};
    VkSubpassDescription subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            2,
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
    check_error(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &pbr_render_pass), vulkan_helper::Error::RENDER_PASS_CREATION_FAILED);
}

void GraphicsModuleVulkanApp::create_sets_layouts() {
    std::array<VkDescriptorSetLayoutBinding, 2> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            2,
            descriptor_set_layout_binding.data()
    };
    vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &pbr_model_data_set_layout);

    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_create_info.bindingCount = 1;
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_binding.data();
    vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &camera_data_set_layout);
}

void GraphicsModuleVulkanApp::load_3d_objects(std::vector<GltfModel> gltf_models) {
    objects.resize(gltf_models.size());

    // Get the total size of the models to allocate a buffer for it and copy them to host memory
    uint64_t models_total_size = 0;
    for (uint32_t i = 0; i < gltf_models.size(); i++) {
        //objects[i].model = std::move(gltf_models[i]);
        objects[i].model = gltf_models[i];
        objects[i].model.copy_model_data_in_ptr(GltfModel::v_model_attributes::V_ALL, true,true,
                                              GltfModel::t_model_attributes::T_ALL, nullptr  );
        if (objects[i].model.get_last_copy_image_layers() != 4) {
            throw(std::make_pair(-1, vulkan_helper::Error::LOADED_MODEL_IS_NOT_PBR));
        }
        models_total_size += objects[i].model.get_last_copy_total_size();
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
    for (uint32_t i=0; i < objects.size(); i++) {
        objects[i].model.copy_model_data_in_ptr(GltfModel::v_model_attributes::V_ALL, true, true,
                                                GltfModel::t_model_attributes::T_ALL, static_cast<uint8_t*>(model_host_data_pointer) + offset);
        offset += objects[i].model.get_last_copy_total_size();
        mesh_and_index_data_size += objects[i].model.get_last_copy_mesh_and_index_data_size();
    }
    vkUnmapMemory(device, host_model_data_memory);

    // After creating memory and buffer for the model data, we need to create a uniform buffer and their memory
    uint64_t uniform_size = vulkan_helper::get_aligned_memory_size(objects.front().model.copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment) * objects.size();
    create_buffer(host_model_uniform_buffer, uniform_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    allocate_and_bind_to_memory(host_model_uniform_memory, {host_model_uniform_buffer}, {}, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(device, host_model_uniform_memory, 0, VK_WHOLE_SIZE, 0, &host_model_uniform_buffer_ptr);

    // After copying the data to host memory, we create a device buffer to hold the mesh and index data
    create_buffer(device_mesh_data_buffer, mesh_and_index_data_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // We also need to create a device buffer for the model's uniform
    create_buffer(device_model_uniform_buffer, uniform_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // We must also create an image for the textures of every object along with their samplers, after destroying the precedent images and samplers
    for (auto &device_model_image : device_model_images) {
        vkDestroyImage(device, device_model_image, nullptr);
    }
    device_model_images.clear();

    for(auto &model_image_sampler : model_image_samplers) {
        vkDestroySampler(device, model_image_sampler, nullptr);
    }
    model_image_samplers.clear();

    for (uint32_t i=0; i<objects.size(); i++) {
        if (objects[i].model.get_last_copy_texture_size()) {
            uint32_t mipmap_levels = vulkan_helper::get_mipmap_count(objects[i].model.get_last_copy_image_extent());

            device_model_images.push_back(VK_NULL_HANDLE);
            create_image(device_model_images.back(), VK_FORMAT_R8G8B8A8_UNORM, objects[i].model.get_last_copy_image_extent(), objects[i].model.get_last_copy_image_layers(),
                         mipmap_levels, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

            model_image_samplers.push_back(VK_NULL_HANDLE);
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
                    VK_TRUE,
                    16.0f,
                    VK_FALSE,
                    VK_COMPARE_OP_ALWAYS,
                    0.0f,
                    static_cast<float>(mipmap_levels),
                    VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                    VK_FALSE,
            };
            check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &model_image_samplers.back()), vulkan_helper::Error::SAMPLER_CREATION_FAILED);
        }
    }
    allocate_and_bind_to_memory(device_model_data_memory, {device_mesh_data_buffer, device_model_uniform_buffer}, device_model_images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // After the buffer and the images have been allocated and binded, we create the image views with 4 layers for every image
    for (auto& device_model_image_view : device_model_images_views) {
        vkDestroyImageView(device, device_model_image_view, nullptr);
    }
    device_model_images_views.clear();

    device_model_images_views.resize(device_model_images.size());
    for (uint32_t i=0; i<device_model_images.size(); i++) {
        create_image_view(device_model_images_views[i], device_model_images[i], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, objects[i].model.get_last_copy_image_layers());
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
    std::vector<VkBufferCopy> buffer_copies(objects.size());
    for (uint32_t i=0; i<buffer_copies.size(); i++) {
        objects[i].data_buffer = device_mesh_data_buffer;

        buffer_copies[i].srcOffset = src_offset;
        src_offset += objects[i].model.get_last_copy_total_size();

        buffer_copies[i].dstOffset = dst_offset;
        objects[i].mesh_data_offset = dst_offset;
        objects[i].index_data_offset = dst_offset + objects[i].model.get_last_copy_mesh_size();
        dst_offset += objects[i].model.get_last_copy_mesh_and_index_data_size();

        buffer_copies[i].size = objects[i].model.get_last_copy_mesh_and_index_data_size();
    }
    vkCmdCopyBuffer(command_buffers[0], host_model_data_buffer, device_mesh_data_buffer, buffer_copies.size(), buffer_copies.data());

    // We need to transition all layers and first mipmap of the device images to DST_OPTIMAL
    std::vector<VkImageMemoryBarrier> image_memory_barriers(device_model_images.size());
    for (uint32_t i=0; i<image_memory_barriers.size(); i++) {
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
    for (uint32_t i=0; i<device_model_images.size(); i++) {
        image_data_offset += objects[i].model.get_last_copy_mesh_and_index_data_size();
        VkBufferImageCopy buffer_image_copy = {
            image_data_offset,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,4},
            {0,0,0},
            objects[i].model.get_last_copy_image_extent()
        };
        image_data_offset += objects[i].model.get_last_copy_texture_size();
        vkCmdCopyBufferToImage(command_buffers[0], host_model_data_buffer, device_model_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
    }

    // After copying the images we need to generate the mipmaps
    for (uint32_t i=0; i < image_memory_barriers.size(); i++) {
        VkImageMemoryBarrier image_memory_barrier;
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = nullptr;

        image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_memory_barrier.image = device_model_images[i];

        image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.baseArrayLayer = 0;
        image_memory_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        VkOffset3D mip_size = { static_cast<int32_t>(objects[i].model.get_last_copy_image_extent().width),
                                static_cast<int32_t>(objects[i].model.get_last_copy_image_extent().height),
                                static_cast<int32_t>(objects[i].model.get_last_copy_image_extent().depth)};

        // We first transition the j-1 mipmap level to SRC_OPTIMAL, perform the copy to the j mipmap level and transition j-1 to SHADER_READ_ONLY
        for (uint32_t j = 1; j < vulkan_helper::get_mipmap_count(objects[i].model.get_last_copy_image_extent()); j++) {
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            image_memory_barrier.subresourceRange.baseMipLevel = j - 1;
            vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

            VkImageBlit image_blit {
                {VK_IMAGE_ASPECT_COLOR_BIT, j - 1, 0, objects[i].model.get_last_copy_image_layers()},
                {{ 0, 0, 0 }, mip_size},
                {VK_IMAGE_ASPECT_COLOR_BIT, j, 0, objects[i].model.get_last_copy_image_layers()},
                {{ 0, 0, 0 }, mip_size.x > 1 ? mip_size.x / 2 : 1, mip_size.y > 1 ? mip_size.y / 2 : 1, 1}
            };
            vkCmdBlitImage(command_buffers[0], device_model_images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, device_model_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

            if (mip_size.x > 1) mip_size.x /= 2;
            if (mip_size.y > 1) mip_size.y /= 2;
        }
    }

    // After the copy we need to transition the last mipmap level (which is still in DST_OPTIMAL) to be shader ready
    for (uint32_t i=0; i<image_memory_barriers.size(); i++) {
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
            {VK_IMAGE_ASPECT_COLOR_BIT, vulkan_helper::get_mipmap_count(objects[i].model.get_last_copy_image_extent()) - 1,
             1, 0, VK_REMAINING_ARRAY_LAYERS}
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
    vkDeviceWaitIdle(device);
    vkResetCommandPool(device, command_pool, 0);
    vkDestroyFence(device, fence, nullptr);
    vkDestroyBuffer(device, host_model_data_buffer, nullptr);
    vkFreeMemory(device, host_model_data_memory, nullptr);
}

void GraphicsModuleVulkanApp::load_lights(const std::vector<Light> &lights) {
    this->lights = lights;

    std::array<VkDescriptorSetLayoutBinding,2> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            static_cast<uint32_t>(lights.size()),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &light_data_set_layout);
}

void GraphicsModuleVulkanApp::set_camera(Camera camera) {
    this->camera = camera;
}

void GraphicsModuleVulkanApp::init_renderer() {
    // We calculate the size needed for the buffer
    uint64_t camera_lights_data_size = vulkan_helper::get_aligned_memory_size(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minStorageBufferOffsetAlignment) +
            lights.front().copy_data_to_ptr(nullptr) * lights.size();

    // We create and allocate a host buffer for holding the camera and lights
    create_buffer(host_camera_lights_uniform_buffer, camera_lights_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    allocate_and_bind_to_memory(host_camera_lights_memory, {host_camera_lights_uniform_buffer}, {}, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    vkMapMemory(device, host_camera_lights_memory, 0, VK_WHOLE_SIZE, 0, &host_camera_lights_data);

    std::vector<VkBuffer> device_buffers_to_allocate;
    std::vector<VkImage> device_images_to_allocate;

    // We create the device buffer that is gonna hold camera and lights information
    create_buffer(device_camera_lights_uniform_buffer, camera_lights_data_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    device_buffers_to_allocate.push_back(device_camera_lights_uniform_buffer);

    std::vector<VkExtent2D> depth_images_resolution(lights.size());
    for (uint32_t i=0; i<depth_images_resolution.size(); i++) {
        depth_images_resolution[i] = {lights[i].get_resolution_from_ratio(1000).x, lights[i].get_resolution_from_ratio(1000).y};
    }
    vsm_context.create_resources(depth_images_resolution, "resources//shaders", pbr_model_data_set_layout, light_data_set_layout);
    smaa_context.create_resources(swapchain_create_info.imageExtent, "resources//shaders");
    hbao_context.create_resources(swapchain_create_info.imageExtent);
    hdr_tonemap_context.create_resources(swapchain_create_info.imageExtent, "resources//shaders", swapchain_images.size());

    // We create some attachments useful during rendering
    create_image(device_depth_image, VK_FORMAT_D32_SFLOAT, {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height, 1},
                 1, 1,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    device_images_to_allocate.push_back(device_depth_image);

    create_image(device_render_target, VK_FORMAT_B10G11R11_UFLOAT_PACK32, {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height, 1},
                 2, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    device_images_to_allocate.push_back(device_render_target);

    create_image(device_normal_g_image, VK_FORMAT_R16G16B16A16_SFLOAT, {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height, 1},
                 1, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    device_images_to_allocate.push_back(device_normal_g_image);

    create_image(device_global_ao_image, VK_FORMAT_R16_SFLOAT, {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height, 1},
                 1, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    device_images_to_allocate.push_back(device_global_ao_image);

    // We request information about the buffer and images we need from vsm_context and store them in a vector
    auto vec = vsm_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), vec.begin(), vec.end());

    // We request information about the images and buffer we need from smaa_context and store them in a vector
    auto smaa_array_images = smaa_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), smaa_array_images.begin(), smaa_array_images.end());

    // We request information about the images and buffer we need from hbao_contex and store them in a vector
    auto hbao_array_images = hbao_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), hbao_array_images.begin(), hbao_array_images.end());

    // We then allocate all needed images and buffers in a single allocation
    allocate_and_bind_to_memory(device_attachments_memory, device_buffers_to_allocate, device_images_to_allocate,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // We then create the image views
    create_image_view(device_depth_image_view, device_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0,1);
    create_image_view(device_render_target_image_views[0], device_render_target, VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
    create_image_view(device_render_target_image_views[1], device_render_target, VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    create_image_view(device_normal_g_image_view, device_normal_g_image, VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
    create_image_view(device_global_ao_image_view, device_global_ao_image, VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

    vsm_context.init_resources();
    smaa_context.init_resources("resources//textures", physical_device_memory_properties, device_render_target_image_views[1], command_pool, command_buffers[0], queue);
    hbao_context.init_resources();

    // After creating all resources we proceed to create the descriptor sets
    write_descriptor_sets();
    create_framebuffers();
    create_pbr_pipeline(); // remove?
    record_command_buffers();
}

void GraphicsModuleVulkanApp::write_descriptor_sets() {
    // Then we get all the required descriptors and request a single pool
    std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> sets_elements_required = {
            {
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 + objects.size()},
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lights.size() + objects.size()},
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
            },
            static_cast<uint32_t>(1 + 1 + objects.size())
    };

    vulkan_helper::insert_or_sum(sets_elements_required, vsm_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, smaa_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, hbao_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, hdr_tonemap_context.get_required_descriptor_pool_size_and_sets());
    std::vector<VkDescriptorPoolSize> descriptor_pool_size = vulkan_helper::convert_map_to_vector(sets_elements_required.first);

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            sets_elements_required.second,
            static_cast<uint32_t>(descriptor_pool_size.size()),
            descriptor_pool_size.data()
    };
    vkDestroyDescriptorPool(device, attachments_descriptor_pool, nullptr);
    vkCreateDescriptorPool(device, &descriptor_pool_create_info, nullptr, &attachments_descriptor_pool);

    // Next we allocate the vsm descriptors in the pool
    vsm_context.allocate_descriptor_sets(attachments_descriptor_pool);
    smaa_context.allocate_descriptor_sets(attachments_descriptor_pool, device_render_target_image_views[0]);
    hbao_context.allocate_descriptor_sets(attachments_descriptor_pool, device_depth_image_view);
    hdr_tonemap_context.allocate_descriptor_sets(attachments_descriptor_pool, device_render_target_image_views[1], device_global_ao_image_view, swapchain_images, swapchain_create_info.imageFormat);

    // then we allocate descriptor sets for camera, lights and objects
    std::vector<VkDescriptorSetLayout> layouts_of_sets;
    layouts_of_sets.push_back(camera_data_set_layout);
    layouts_of_sets.push_back(light_data_set_layout);
    layouts_of_sets.insert(layouts_of_sets.end(), objects.size(), pbr_model_data_set_layout);

    descriptor_sets.resize(layouts_of_sets.size());
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            attachments_descriptor_pool,
            static_cast<uint32_t>(layouts_of_sets.size()),
            layouts_of_sets.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    // After we write the descriptor sets for camera, lights and objects
    std::vector<VkWriteDescriptorSet> write_descriptor_set(1 + 2 + 2 * objects.size());

    // First we do the camera
    VkDescriptorBufferInfo camera_descriptor_buffer_info = {
            device_camera_lights_uniform_buffer,
            0,
            camera.copy_data_to_ptr(nullptr)
    };
    write_descriptor_set[0] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            descriptor_sets[0],
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            nullptr,
            &camera_descriptor_buffer_info,
            nullptr
    };

    // Then the lights
    VkDescriptorBufferInfo light_descriptor_buffer_info = {
        device_camera_lights_uniform_buffer,
        vulkan_helper::get_aligned_memory_size(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minStorageBufferOffsetAlignment),
        lights.front().copy_data_to_ptr(nullptr) * lights.size()
    };
    std::vector<VkDescriptorImageInfo> light_descriptor_image_infos(lights.size());
    for(uint32_t i=0; i<lights.size(); i++) {
        light_descriptor_image_infos[i] = {
                max_aniso_linear_sampler,
                vsm_context.get_image_view(i),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    write_descriptor_set[1] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        descriptor_sets[1],
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        nullptr,
        &light_descriptor_buffer_info,
        nullptr
    };
    write_descriptor_set[2] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        descriptor_sets[1],
        1,
        0,
        static_cast<uint32_t>(light_descriptor_image_infos.size()),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        light_descriptor_image_infos.data(),
        nullptr,
        nullptr
    };

    // Lastly, the objects
    std::vector<VkDescriptorBufferInfo> object_descriptor_buffer_infos(objects.size());
    for(uint32_t i = 0, offset = 0; i<objects.size(); i++) {
        object_descriptor_buffer_infos[i] = {
                device_model_uniform_buffer,
                offset,
                vulkan_helper::get_aligned_memory_size(objects[i].model.copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment)
        };
        offset += object_descriptor_buffer_infos[i].range;
    }
    std::vector<VkDescriptorImageInfo> object_descriptor_image_infos(objects.size());
    for(uint32_t i = 0; i<objects.size(); i++) {
        object_descriptor_image_infos[i] = {
                model_image_samplers[i],
                device_model_images_views[i],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    for (uint32_t i=0; i<objects.size(); i++) {
        write_descriptor_set[2*i+3] = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                descriptor_sets[i+2],
                0,
                0,
                1,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                nullptr,
                &object_descriptor_buffer_infos[i],
                nullptr
        };
        write_descriptor_set[2*i+4] = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                descriptor_sets[i+2],
                1,
                0,
                1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                &object_descriptor_image_infos[i],
                nullptr,
                nullptr
        };
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}

void GraphicsModuleVulkanApp::create_framebuffers() {
    std::array<VkImageView, 3> attachments {device_depth_image_view, device_render_target_image_views[0], device_normal_g_image_view};
    VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            pbr_render_pass,
            attachments.size(),
            attachments.data(),
            this->swapchain_create_info.imageExtent.width,
            this->swapchain_create_info.imageExtent.height,
            1
    };
    vkDestroyFramebuffer(device, pbr_framebuffer, nullptr);
    check_error(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &pbr_framebuffer), vulkan_helper::Error::FRAMEBUFFER_CREATION_FAILED);
}

void GraphicsModuleVulkanApp::create_pbr_pipeline() {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content("resources//shaders//pbr.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content("resources//shaders//pbr.frag.spv", shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

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

    VkVertexInputBindingDescription vertex_input_binding_description = {
            0,
            12 * sizeof(float),
            VK_VERTEX_INPUT_RATE_VERTEX
    };
    std::array<VkVertexInputAttributeDescription,4> vertex_input_attribute_description  {{
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

    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(swapchain_create_info.imageExtent.width),
            static_cast<float>(swapchain_create_info.imageExtent.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height}
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

    std::array<VkPipelineColorBlendAttachmentState, 2> pipeline_color_blend_attachment_state;
    pipeline_color_blend_attachment_state[0] = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    pipeline_color_blend_attachment_state[1] = {
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
            pipeline_color_blend_attachment_state.size(),
            pipeline_color_blend_attachment_state.data(),
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

    std::array<VkDescriptorSetLayout,3> descriptor_set_layouts = {pbr_model_data_set_layout, light_data_set_layout, camera_data_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layouts.size(),
            descriptor_set_layouts.data(),
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, pbr_pipeline_layout, nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pbr_pipeline_layout);

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
            &pipeline_dynamic_state_create_info,
            pbr_pipeline_layout,
            pbr_render_pass,
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, pbr_pipeline, nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pbr_pipeline);

    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void GraphicsModuleVulkanApp::record_command_buffers() {
    vkResetCommandPool(device, command_pool, 0);

    for (uint32_t i = 0; i < swapchain_images.size(); i++) {
        VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, nullptr };
        vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info);

        VkBufferCopy buffer_copy = { 0,0,vulkan_helper::get_aligned_memory_size(objects.front().model.copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment) * objects.size()};
        vkCmdCopyBuffer(command_buffers[i], host_model_uniform_buffer, device_model_uniform_buffer, 1, &buffer_copy);

        buffer_copy = {0,0, vulkan_helper::get_aligned_memory_size(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minStorageBufferOffsetAlignment) +
                            lights.front().copy_data_to_ptr(nullptr) * lights.size()};
        vkCmdCopyBuffer(command_buffers[i], host_camera_lights_uniform_buffer, device_camera_lights_uniform_buffer, 1, &buffer_copy);

        // Transitioning layout from the write to the shader read
        std::array<VkBufferMemoryBarrier,2> buffer_memory_barriers;
        buffer_memory_barriers[0] = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_UNIFORM_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_model_uniform_buffer,
                0,
                VK_WHOLE_SIZE
        };
        buffer_memory_barriers[1] = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_UNIFORM_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_camera_lights_uniform_buffer,
                0,
                VK_WHOLE_SIZE
        };
        vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 2, buffer_memory_barriers.data(), 0, nullptr);

        std::vector<VkDescriptorSet> object_descriptor_sets(descriptor_sets.begin() + 2, descriptor_sets.end());
        vsm_context.record_into_command_buffer(command_buffers[i], object_descriptor_sets, descriptor_sets[1], objects);

        std::array<VkClearValue,3> clear_values;
        clear_values[0].depthStencil = {1.0f, 0};
        clear_values[1].color = {0.0f, 0.0f, 0.0f, 0.0f};
        clear_values[2].color = {0.0f, 0.0f, 0.0f, 0.0f};

        VkRenderPassBeginInfo render_pass_begin_info = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                nullptr,
                pbr_render_pass,
                pbr_framebuffer,
                {{0,0},{swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height}},
                clear_values.size(),
                clear_values.data()
        };
        vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline);
        VkViewport viewport = {
                0.0f,
                0.0f,
                static_cast<float>(swapchain_create_info.imageExtent.width),
                static_cast<float>(swapchain_create_info.imageExtent.height),
                0.0f,
                1.0f
        };
        vkCmdSetViewport(command_buffers[i], 0, 1, &viewport);

        VkRect2D scissor = {
                {0,0},
                {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height}
        };
        vkCmdSetScissor(command_buffers[i], 0, 1, &scissor);

        for (uint32_t j=0; j<objects.size(); j++) {
            std::array<VkDescriptorSet,3> to_bind = { object_descriptor_sets[j], descriptor_sets[1], descriptor_sets[0] };
            vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline_layout, 0, to_bind.size(), to_bind.data(), 0, nullptr);

            vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &objects[j].data_buffer, &objects[j].mesh_data_offset);
            vkCmdBindIndexBuffer(command_buffers[i], objects[j].data_buffer, objects[j].index_data_offset, objects[j].model.get_last_copy_index_type());
            vkCmdDrawIndexed(command_buffers[i], objects[j].model.get_last_copy_indices(), 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(command_buffers[i]);

        smaa_context.record_into_command_buffer(command_buffers[i]);
        hbao_context.record_into_command_buffer(command_buffers[i], swapchain_create_info.imageExtent, camera.znear, camera.zfar, static_cast<bool>(camera.pos[3]));

        // This barrier is temporary! TODO: remove this barrier
        VkImageMemoryBarrier image_memory_barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_MEMORY_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                device_global_ao_image,
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

        hdr_tonemap_context.record_into_command_buffer(command_buffers[i], i, {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height});

        vkEndCommandBuffer(command_buffers[i]);
    }
}

void GraphicsModuleVulkanApp::start_frame_loop(std::function<void(GraphicsModuleVulkanApp*)> resize_callback,
                                               std::function<void(GraphicsModuleVulkanApp*, uint32_t)> frame_start) {
    uint32_t rendered_frames = 0;
    std::chrono::steady_clock::time_point frames_time, current_frame, last_frame;
    uint32_t delta_time;

    while (!glfwWindowShouldClose(window)) {
        current_frame = std::chrono::steady_clock::now();
        delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_frame - last_frame).count();
        last_frame = current_frame;

        glfwPollEvents();
        frame_start(this, delta_time);

        camera.copy_data_to_ptr(static_cast<uint8_t*>(host_camera_lights_data));
        for(uint32_t i = 0, offset = vulkan_helper::get_aligned_memory_size(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minStorageBufferOffsetAlignment);
            i<lights.size(); i++) {
            offset += lights[i].copy_data_to_ptr(static_cast<uint8_t*>(host_camera_lights_data) + offset);
        }

        for(uint32_t i = 0, offset = 0; i < objects.size(); i++) {
            objects[i].model.copy_uniform_data(static_cast<uint8_t*>(host_model_uniform_buffer_ptr) + offset);
            offset += vulkan_helper::get_aligned_memory_size(objects[i].model.copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment);
        }

        uint32_t image_index = 0;
        VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphores[0], VK_NULL_HANDLE, &image_index);
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
            on_window_resize(resize_callback);
            continue;
        }
        else if (res != VK_SUCCESS) {
            check_error(res, vulkan_helper::Error::ACQUIRE_NEXT_IMAGE_FAILED);
        }

        vkDeviceWaitIdle(device);
        this->record_command_buffers();

        VkPipelineStageFlags pipeline_stage_flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkSubmitInfo submit_info = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO,
                nullptr,
                1,
                &semaphores[0],
                &pipeline_stage_flags,
                1,
                &command_buffers[image_index],
                1,
                &semaphores[1]
        };
        check_error(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), vulkan_helper::Error::QUEUE_SUBMIT_FAILED);

        VkPresentInfoKHR present_info = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                nullptr,
                1,
                &semaphores[1],
                1,
                &swapchain,
                &image_index,
                nullptr
        };
        res = vkQueuePresentKHR(queue, &present_info);
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
            on_window_resize(resize_callback);
            continue;
        }
        else if (res != VK_SUCCESS) {
            check_error(res, vulkan_helper::Error::QUEUE_PRESENT_FAILED);
        }

        rendered_frames++;
        uint32_t time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - frames_time).count();
        if ( time_diff > 1000) {
            std::cout << "Msec/frame: " << ( time_diff / static_cast<float>(rendered_frames)) << std::endl;
            rendered_frames = 0;
            frames_time = std::chrono::steady_clock::now();
        }
    }
}

void GraphicsModuleVulkanApp::on_window_resize(std::function<void(GraphicsModuleVulkanApp*)> resize_callback) {
    vkDeviceWaitIdle(device);
    create_swapchain();
    init_renderer();
    resize_callback(this);
}

GraphicsModuleVulkanApp::~GraphicsModuleVulkanApp() {
    vkDeviceWaitIdle(device);

    for (auto& semaphore : semaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }

    vkDestroyPipelineLayout(device, pbr_pipeline_layout, nullptr);
    vkDestroyPipeline(device, pbr_pipeline, nullptr);

    vkDestroyFramebuffer(device, pbr_framebuffer, nullptr);
    vkDestroyRenderPass(device, pbr_render_pass, nullptr);

    vkDestroySampler(device, max_aniso_linear_sampler, nullptr);

    for(auto &model_image_sampler : model_image_samplers) {
        vkDestroySampler(device, model_image_sampler, nullptr);
    }
    model_image_samplers.clear();

    // Model uniform related things freed
    vkUnmapMemory(device, host_model_uniform_memory);
    vkDestroyBuffer(device, host_model_uniform_buffer, nullptr);
    vkFreeMemory(device, host_model_uniform_memory, nullptr);

    // Model related things freed
    for (auto& device_model_image_view : device_model_images_views) {
        vkDestroyImageView(device, device_model_image_view, nullptr);
    }
    for (auto& device_model_image : device_model_images) {
        vkDestroyImage(device, device_model_image, nullptr);
    }
    vkDestroyBuffer(device, device_mesh_data_buffer, nullptr);
    vkDestroyBuffer(device, device_model_uniform_buffer, nullptr);
    vkFreeMemory(device, device_model_data_memory, nullptr);

    // Camera and light uniform related things freed
    vkUnmapMemory(device, host_camera_lights_memory);
    vkDestroyBuffer(device, host_camera_lights_uniform_buffer, nullptr);
    vkFreeMemory(device, host_camera_lights_memory, nullptr);

    vkDestroyImageView(device, device_depth_image_view, nullptr);
    vkDestroyImage(device, device_depth_image, nullptr);
    for (auto &device_render_target_image_view : device_render_target_image_views) {
        vkDestroyImageView(device, device_render_target_image_view, nullptr);
    }
    vkDestroyImage(device, device_render_target, nullptr);
    vkDestroyImageView(device, device_normal_g_image_view, nullptr);
    vkDestroyImage(device, device_normal_g_image, nullptr);
    vkDestroyImageView(device, device_global_ao_image_view, nullptr);
    vkDestroyImage(device, device_global_ao_image, nullptr);
    vkDestroyBuffer(device, device_camera_lights_uniform_buffer, nullptr);
    vkFreeMemory(device, device_attachments_memory, nullptr);

    vkDestroyDescriptorSetLayout(device, pbr_model_data_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, light_data_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, camera_data_set_layout, nullptr);
    vkDestroyDescriptorPool(device, attachments_descriptor_pool, nullptr);
}

// ----------------- Helper methods -----------------
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

void GraphicsModuleVulkanApp::create_image(VkImage &image, VkFormat format, VkExtent3D image_size, uint32_t layers, uint32_t mipmaps_count, VkImageUsageFlags usage_flags, VkImageCreateFlags create_flags) {
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            create_flags,
            VK_IMAGE_TYPE_2D,
            format,
            image_size,
            mipmaps_count,
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
            layer_count == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            image_format,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            {aspect_mask, 0, VK_REMAINING_MIP_LEVELS, start_layer, layer_count}
    };
    vkDestroyImageView(device, image_view, nullptr);
    check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &image_view), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
}

void GraphicsModuleVulkanApp::allocate_and_bind_to_memory(VkDeviceMemory &memory, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VkMemoryPropertyFlags flags) {
    std::vector<VkMemoryRequirements> memory_requirements(buffers.size() + images.size());
    for (uint32_t i=0; i<buffers.size(); i++) {
        vkGetBufferMemoryRequirements(device, buffers[i], &memory_requirements[i]);
    }
    for (uint32_t i=0; i<images.size(); i++) {
        vkGetImageMemoryRequirements(device, images[i], &memory_requirements[i+buffers.size()]);
    }

    uint64_t allocation_size = 0;
    for (uint32_t i = 0; i < memory_requirements.size(); i++) {
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

    auto sum_offsets = [&](uint32_t idx) {
        int sum = 0;
        for (uint32_t i = 0; i < idx; i++) {
            sum += memory_requirements[i].size;
            if ((i+1) < memory_requirements.size()) {
                sum = vulkan_helper::get_aligned_memory_size(sum, memory_requirements.at(i+1).alignment);
            }
        }
        return sum;
    };

    for (uint32_t i=0; i<buffers.size(); i++) {
        check_error(vkBindBufferMemory(device, buffers[i], memory, sum_offsets(i)), vulkan_helper::Error::BIND_BUFFER_MEMORY_FAILED);
    }
    for (uint32_t i=0; i<images.size(); i++) {
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