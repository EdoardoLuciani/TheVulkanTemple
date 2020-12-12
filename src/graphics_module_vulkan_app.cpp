#include "graphics_module_vulkan_app.h"
#include <vector>
#include "vulkan_helper.h"

void GraphicsModuleVulkanApp::load_3d_objects(std::vector<std::string> model_path, uint32_t object_matrix_size) {
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    std::vector<tinygltf::Model> models(model_path.size());

    // Get the total size of the models to allocate a buffer for it and copy them to host memory
    uint64_t models_total_size = 0;
    for (int i=0; i < model_path.size(); i++) {
        vulkan_helper::model_data_info model_data;
		check_error(!loader.LoadBinaryFromFile(&models[i], &err, &warn, model_path[i]), Error::MODEL_LOADING_FAILED);
        vulkan_helper::copy_gltf_contents(models[i],
                                          vulkan_helper::v_model_attributes::V_ALL,
                                          true,
                                          true,
                                          vulkan_helper::t_model_attributes::T_ALL,
                                          nullptr, model_data);
        models_total_size += vulkan_helper::get_model_data_total_size(model_data);
    }

    VkBufferCreateInfo buffer_create_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        models_total_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    VkBuffer model_data_buffer;
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &model_data_buffer), Error::BUFFER_CREATION_FAILED);
    VkMemoryRequirements model_data_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(device, model_data_buffer, &model_data_buffer_memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        model_data_buffer_memory_requirements.size,
        vulkan_helper::select_memory_index(physical_device_memory_properties, model_data_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    };
    VkDeviceMemory host_model_data_memory;
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &host_model_data_memory), Error::MEMORY_ALLOCATION_FAILED);

    check_error(vkBindBufferMemory(device, model_data_buffer, host_model_data_memory, 0), Error::BIND_BUFFER_MEMORY_FAILED);

    void *model_host_data_pointer;
    check_error(vkMapMemory(device, host_model_data_memory, 0, VK_WHOLE_SIZE, 0, &model_host_data_pointer), Error::POINTER_REQUEST_FOR_HOST_MEMORY_FAILED);

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
        mesh_and_index_data_size += model_data[i].interleaved_mesh_data_size + model_data[i].index_data_size;
    }

    VkMappedMemoryRange mapped_memory_range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, host_model_data_memory, 0, VK_WHOLE_SIZE};
    check_error(vkFlushMappedMemoryRanges(device, 1, &mapped_memory_range), Error::MAPPED_MEMORY_FLUSH_FAILED);

    // After copying the data to host memory, we create a device buffer to hold the mesh and index data
    buffer_create_info = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        mesh_and_index_data_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    vkDestroyBuffer(device, device_mesh_data_buffer, nullptr);
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &device_mesh_data_buffer), Error::BUFFER_CREATION_FAILED);

    // We must also create images for the textures of every object
    for(auto& device_model_image : device_model_images) {
        vkDestroyImage(device, device_model_image, nullptr);
    }
    device_model_images.resize(models.size());
    for (int i=0; i<device_model_images.size(); i++) {
        VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM,
            model_data[i].image_size,
            1,
            model_data[i].image_layers,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED
        };
        check_error(vkCreateImage(device, &image_create_info, nullptr, &device_model_images[i]), Error::IMAGE_CREATION_FAILED);
    }

    // Then we need the memory requirements for both the mesh_data_buffer and the images in order to allocate memory
    std::vector<VkMemoryRequirements> device_memory_requirements(1+device_model_images.size());
    vkGetBufferMemoryRequirements(device, device_mesh_data_buffer, &device_memory_requirements[0]);
    for(int i=0; i<device_model_images.size(); i++) {
        vkGetImageMemoryRequirements(device, device_model_images[i], &device_memory_requirements[1+i]);
    }

    // Do I even need this???
    uint64_t allocation_size = 0;
    for (int i = 0; i < device_memory_requirements.size(); i++) {
        allocation_size += device_memory_requirements[i].size;
        if ((i+1) < device_memory_requirements.size()) {
            uint32_t align = device_memory_requirements.at(i+1).alignment;
            allocation_size += vulkan_helper::get_alignment_memory(allocation_size, align);
        }
    }

    memory_allocate_info = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        allocation_size,
        vulkan_helper::select_memory_index(physical_device_memory_properties,device_memory_requirements[1],VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    vkFreeMemory(device, device_model_data_memory, nullptr);
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &device_model_data_memory), Error::MEMORY_ALLOCATION_FAILED);

    auto sum_offsets = [&](int idx) {
        int sum = 0;
        for (int i = 0; i < idx; i++) {
            sum += device_memory_requirements[i].size;
            if ((i+1) < device_memory_requirements.size()) {
                uint32_t align = device_memory_requirements.at(i+1).alignment;
                sum += vulkan_helper::get_alignment_memory(sum, align);
            }
        }
        return sum;
    };

    vkBindBufferMemory(device, device_mesh_data_buffer, device_model_data_memory, 0);
    for (int i=0; i<device_model_images.size(); i++) {
        vkBindImageMemory(device, device_model_images[i], device_model_data_memory, sum_offsets(1+i));
    }

    // After the buffer and the images have been allocated and binded, we process to create the image views
    for (auto& device_model_image_views : device_model_images_views) {
        for (auto & device_model_image_view : device_model_image_views) {
            vkDestroyImageView(device, device_model_image_view, nullptr);
        }
    }

    // Here when creating the images views we say that the first image view of an image is of type srgb and the other ones are unorm
    device_model_images_views.resize(device_model_images.size());
    for (uint32_t i=0; i<device_model_images.size(); i++) {
        device_model_images_views[i].resize(model_data[i].image_layers);
        for (uint32_t j=0; j<device_model_images_views.size(); j++) {
            VkFormat image_format;
            if (j == 0) {
                image_format = VK_FORMAT_R8G8B8A8_SRGB;
            }
            else {
                image_format = VK_FORMAT_R8G8B8A8_UNORM;
            }
            VkImageViewCreateInfo image_view_create_info = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                nullptr,
                0,
                device_model_images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                image_format,
                {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, j, 1}
            };
            vkCreateImageView(device, &image_view_create_info, nullptr, &device_model_images_views[i][j]);
        }
    }

    // TODO: create buffer for the uniform of every object
}

GraphicsModuleVulkanApp::~GraphicsModuleVulkanApp() {
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
