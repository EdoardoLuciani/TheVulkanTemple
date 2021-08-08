#include "graphics_module_vulkan_app.h"
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <span>
#include "layers/smaa/smaa_context.h"
#include "layers/pbr/pbr_context.h"
#include "layers/vsm/vsm_context.h"
#include "layers/hbao/hbao_context.h"
#include "layers/hdr_tonemap/hdr_tonemap_context.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/combine.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/assign.hpp>
#include <boost/foreach.hpp>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION
#include "external/vk_mem_alloc.h"

#include "vulkan_helper.h"
#include <unordered_map>

GraphicsModuleVulkanApp::GraphicsModuleVulkanApp(const std::string &application_name,
                                                 VkExtent2D window_size,
                                                 bool fullscreen,
                                                 EngineOptions options) :
                         BaseVulkanApp(application_name,
                                       get_instance_extensions(),
                                       window_size,
                                       fullscreen,
                                       get_device_extensions(),
                                       get_required_physical_device_features(false, options),
                                       VK_TRUE),
                         vsm_context(device, "resources//shaders"),
                         pbr_context(device, physical_device_memory_properties, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R8G8B8A8_UNORM),
                         smaa_context(device, VK_FORMAT_B10G11R11_UFLOAT_PACK32),
                         hbao_context(device, physical_device_memory_properties, window_size, VK_FORMAT_D32_SFLOAT, VK_FORMAT_R8_UNORM, "resources//shaders", false),
                         hdr_tonemap_context(device, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8B8A8_UNORM) {
    engine_options = options;

	// Deleting the physical device feature
    get_required_physical_device_features(true, options);

    if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		amd_fsr = std::make_unique<AmdFsr>(device, engine_options.fsr_settings, "resources//shaders");
		rendering_resolution = amd_fsr->get_recommended_input_resolution(swapchain_create_info.imageExtent);
    }
    else {
		rendering_resolution = swapchain_create_info.imageExtent;
    }

    VmaAllocatorCreateInfo vma_allocator_create_info = {0};
    vma_allocator_create_info.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    vma_allocator_create_info.physicalDevice = this->selected_physical_device;
    vma_allocator_create_info.device = this->device;
    vma_allocator_create_info.preferredLargeHeapBlockSize = 512000000; // blocks larger than 512 MB get allocated in a separate VkDeviceMemory

    VmaVulkanFunctions vma_vulkan_functions = {
            vkGetPhysicalDeviceProperties,
            vkGetPhysicalDeviceMemoryProperties,
            vkAllocateMemory,
            vkFreeMemory,
            vkMapMemory,
            vkUnmapMemory,
            vkFlushMappedMemoryRanges,
            vkInvalidateMappedMemoryRanges,
            vkBindBufferMemory,
            vkBindImageMemory,
            vkGetBufferMemoryRequirements,
            vkGetImageMemoryRequirements,
            vkCreateBuffer,
            vkDestroyBuffer,
            vkCreateImage,
            vkDestroyImage,
            vkCmdCopyBuffer,
            vkGetBufferMemoryRequirements2,
            vkGetImageMemoryRequirements2,
            vkBindBufferMemory2,
            vkBindImageMemory2,
            vkGetPhysicalDeviceMemoryProperties2
    };
    vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_functions;
    vma_allocator_create_info.instance = this->instance;
    vma_allocator_create_info.vulkanApiVersion = VK_MAKE_VERSION(1, 1, 0);
    vmaCreateAllocator(&vma_allocator_create_info, &vma_allocator);

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

    create_sets_layouts();
    pbr_context.create_pipeline("resources//shaders", pbr_model_data_set_layout, camera_data_set_layout, light_data_set_layout);

    // We perform allocations that are not dependent on screen resolutions
    if (amd_fsr) {
		allocate_and_bind_to_memory_buffer(amd_fsr_uniform_allocation, amd_fsr->get_device_buffer(), VMA_MEMORY_USAGE_GPU_ONLY);
    }
}

std::vector<const char*> GraphicsModuleVulkanApp::get_instance_extensions() {
    std::vector<const char*> instance_extensions = {"VK_KHR_surface"};
    #ifdef _WIN64
        instance_extensions.push_back("VK_KHR_win32_surface")
    #elif __linux__
        #ifdef VK_USE_PLATFORM_WAYLAND_KHR
            instance_extensions.push_back("VK_KHR_wayland_surface")
        #else
            instance_extensions.push_back("VK_KHR_xlib_surface");
        #endif
    #else
        #error "Unknown compiler or not supported OS"
    #endif

    return instance_extensions;
}

std::vector<const char*> GraphicsModuleVulkanApp::get_device_extensions() {
    return {"VK_KHR_swapchain", "VK_EXT_descriptor_indexing", "VK_EXT_memory_budget"};
}

VkPhysicalDeviceFeatures2* GraphicsModuleVulkanApp::get_required_physical_device_features(bool delete_static_structure, EngineOptions engine_options) {
    static VkPhysicalDeviceFeatures2 *required_device_features2 = nullptr;

    // Structure needs to be deleted
    if (delete_static_structure && (required_device_features2 != nullptr)) {
        vulkan_helper::free_physical_device_feature_struct_chain(required_device_features2);
        required_device_features2 = nullptr;
        return required_device_features2;
    }
    // Structure has already been created
    else if (required_device_features2 != nullptr) {
        return required_device_features2;
    }

    // First time structure creation
    else {
    	VkPhysicalDeviceMultiviewFeatures *required_physical_device_multiview_features = new VkPhysicalDeviceMultiviewFeatures();
		required_physical_device_multiview_features->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
		required_physical_device_multiview_features->multiview = VK_TRUE;

        void* p_next;
        // AmdFsr can be run in F32 or F16, in the latter storageBuffer16BitAccess and shaderFloat16 need to be enabled
        if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE && engine_options.fsr_settings.precision == AmdFsr::Precision::FP16) {
            VkPhysicalDevice16BitStorageFeatures *physical_device_16_bit_storage_features = new VkPhysicalDevice16BitStorageFeatures();
            physical_device_16_bit_storage_features->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
            physical_device_16_bit_storage_features->pNext = required_physical_device_multiview_features;
            physical_device_16_bit_storage_features->storageBuffer16BitAccess = VK_TRUE;

            VkPhysicalDeviceShaderFloat16Int8FeaturesKHR *physical_device_shader_float_16_int_8_features = new VkPhysicalDeviceShaderFloat16Int8FeaturesKHR({
					VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
					physical_device_16_bit_storage_features,
					VK_TRUE, // shaderFloat16
                	VK_FALSE // shaderInt8
            });
            p_next = physical_device_shader_float_16_int_8_features;
        }
        else {
            p_next = required_physical_device_multiview_features;
        }

        VkPhysicalDeviceDescriptorIndexingFeaturesEXT *required_physical_device_indexing_features = new VkPhysicalDeviceDescriptorIndexingFeaturesEXT();
        required_physical_device_indexing_features->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
        required_physical_device_indexing_features->pNext = p_next;
        required_physical_device_indexing_features->runtimeDescriptorArray = VK_TRUE;
        required_physical_device_indexing_features->shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        required_physical_device_indexing_features->descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        required_physical_device_indexing_features->descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        required_physical_device_indexing_features->descriptorBindingPartiallyBound = VK_TRUE;

        required_device_features2 = new VkPhysicalDeviceFeatures2();
        required_device_features2->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        required_device_features2->pNext = required_physical_device_indexing_features;
        required_device_features2->features.samplerAnisotropy = VK_TRUE;
		if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE && engine_options.fsr_settings.precision == AmdFsr::Precision::FP16) {
			required_device_features2->features.shaderInt16 = VK_TRUE;
		}
        return required_device_features2;
    }
}

void GraphicsModuleVulkanApp::create_sets_layouts() {
    // Creating the descriptor set layout for the model data
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

    // Creating the descriptor set layout for the light
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

    // Creating the descriptor set layout for the light
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
            MAX_SHADOWED_LIGHTS,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
    };
    // Here we indicate that the first and second descriptor contents can be updated after they have been binded
    std::array<VkDescriptorBindingFlagsEXT,2> descriptor_binding_flags = {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descriptor_set_layout_binding_flags_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
            nullptr,
            descriptor_binding_flags.size(),
            descriptor_binding_flags.data()
    };
    descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            &descriptor_set_layout_binding_flags_create_info,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &light_data_set_layout);
}

void GraphicsModuleVulkanApp::load_3d_objects(std::vector<std::pair<std::string, glm::mat4>> model_file_matrix) {
    std::vector<GltfModel> gltf_models;

    // Get the total size of the models to allocate a buffer for it and copy them to host memory
    uint64_t models_total_size = 0;
    for (uint32_t i = 0; i < model_file_matrix.size(); i++) {
		gltf_models.push_back(GltfModel(model_file_matrix[i].first));
		auto infos = gltf_models.back().copy_model_data_in_ptr(GltfModel::v_model_attributes::V_ALL, false, true,
                                              GltfModel::t_model_attributes::T_ALL, nullptr);

		vk_models.emplace_back(VkModel(device, model_file_matrix[i].first, infos, model_file_matrix[i].second));
        models_total_size += vk_models.back().get_all_primitives_total_size();
    }
    VkBuffer host_model_data_buffer = VK_NULL_HANDLE;
    create_buffer(host_model_data_buffer, models_total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VmaAllocation host_model_data_transient_allocation = VK_NULL_HANDLE;
    allocate_and_bind_to_memory_buffer(host_model_data_transient_allocation, host_model_data_buffer, VMA_MEMORY_USAGE_CPU_ONLY);
    void *model_host_data_pointer;
    vmaMapMemory(this->vma_allocator, host_model_data_transient_allocation, &model_host_data_pointer);

    // After getting a pointer for the host memory, we iterate once again in the models to copy them to memory and store some information about them
    uint64_t offset = 0;
    uint64_t mesh_and_index_data_size = 0;
    for (uint32_t i=0; i < gltf_models.size(); i++) {
        gltf_models[i].copy_model_data_in_ptr(GltfModel::v_model_attributes::V_ALL, true, true,
                                                GltfModel::t_model_attributes::T_ALL, static_cast<uint8_t*>(model_host_data_pointer) + offset);
        offset += vk_models[i].get_all_primitives_total_size();
        // The mesh in the device buffer needs to be aligned to the attribute first size, which is always the position hence 12
        for (const auto& host_info : vk_models[i].host_primitives_data_info) {
        	mesh_and_index_data_size = vulkan_helper::get_aligned_memory_size(mesh_and_index_data_size, 12) + host_info.get_mesh_and_index_data_size();
        }
    }
    vmaUnmapMemory(this->vma_allocator, host_model_data_transient_allocation);

    // After creating memory and buffer for the model data, we need to create a uniform buffer and their memory
    uint64_t uniform_size = vulkan_helper::get_aligned_memory_size(vk_models.front().copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment) * vk_models.size();
    create_buffer(host_model_uniform_buffer, uniform_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    allocate_and_bind_to_memory_buffer(host_model_uniform_allocation, host_model_uniform_buffer, VMA_MEMORY_USAGE_CPU_ONLY);
    vmaMapMemory(this->vma_allocator, host_model_uniform_allocation, &host_model_uniform_buffer_ptr);

    // After copying the data to host memory, we create a device buffer to hold the mesh and index data
    create_buffer(device_mesh_data_buffer, mesh_and_index_data_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // We also need to create a device buffer for the model's uniform
    create_buffer(device_model_uniform_buffer, uniform_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	std::vector<VmaAllocation> out_allocations = {device_mesh_data_allocation, device_model_uniform_allocation};
    allocate_and_bind_to_memory(out_allocations, {device_mesh_data_buffer, device_model_uniform_buffer}, {}, VMA_MEMORY_USAGE_GPU_ONLY);
    device_mesh_data_allocation = out_allocations[0];
    device_model_uniform_allocation = out_allocations[1];

    for (uint32_t i = 0; i < vk_models.size(); i++) {
    	vk_models[i].vk_create_images(amd_fsr ? amd_fsr->get_negative_mip_bias() : 0.0f, vma_allocator);
    }

    // After setting the required memory we register the command buffer to upload the data
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        nullptr
    };
    vkBeginCommandBuffer(command_buffers[0], &command_buffer_begin_info);

    VkBufferMemoryBarrier buffer_memory_barrier = {
    		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    		nullptr,
    		0,
    		VK_ACCESS_TRANSFER_WRITE_BIT,
    		VK_QUEUE_FAMILY_IGNORED,
    		VK_QUEUE_FAMILY_IGNORED,
    		device_mesh_data_buffer,
    		0,
    		VK_WHOLE_SIZE
    };
    vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);

    // We calculate the offset for the mesh data both in the host and device buffers and register the copy
    uint64_t src_offset = 0;
    uint64_t dst_offset = 0;
    std::vector<VkBufferCopy> buffer_copies;
	for (uint32_t i = 0; i < vk_models.size(); i++) {
		for (uint32_t j = 0; j < vk_models[i].device_primitives_data_info.size(); j++) {
			vk_models[i].device_primitives_data_info[j].data_buffer = device_mesh_data_buffer;

			VkBufferCopy buffer_copy;
			buffer_copy.srcOffset = src_offset;
			src_offset += vk_models[i].host_primitives_data_info[j].get_total_size();

			dst_offset = vulkan_helper::get_aligned_memory_size(dst_offset, 12);
			buffer_copy.dstOffset = dst_offset;
			vk_models[i].device_primitives_data_info[j].primitive_vertices_data_offset = dst_offset;
			vk_models[i].device_primitives_data_info[j].index_data_offset = dst_offset + vk_models[i].host_primitives_data_info[j].interleaved_vertices_data_size;
			dst_offset += vk_models[i].host_primitives_data_info[j].get_mesh_and_index_data_size();

			buffer_copy.size = vk_models[i].host_primitives_data_info[j].get_mesh_and_index_data_size();

			buffer_copies.push_back(buffer_copy);
		}
	}
    vkCmdCopyBuffer(command_buffers[0], host_model_data_buffer, device_mesh_data_buffer, buffer_copies.size(), buffer_copies.data());

	buffer_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	buffer_memory_barrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);

	dst_offset = 0;
	for (uint32_t i = 0; i < vk_models.size(); i++) {
		dst_offset = vk_models[i].vk_init_images(command_buffers[0], host_model_data_buffer, dst_offset);
	}
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
    vmaFreeMemory(this->vma_allocator, host_model_data_transient_allocation);
}

void GraphicsModuleVulkanApp::load_lights(std::vector<Light> &&lights) {
    this->lights_container.assign(lights.begin(), lights.end());
}

void GraphicsModuleVulkanApp::set_camera(Camera &&camera) {
    this->camera = camera;
}

void GraphicsModuleVulkanApp::init_renderer() {
    // We calculate the size needed for the buffer
    uint64_t camera_lights_data_size = vulkan_helper::get_aligned_memory_size(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minStorageBufferOffsetAlignment) +
            lights_container.front().copy_data_to_ptr(nullptr) * lights_container.size();

    // We create and allocate a host buffer for holding the camera and lights
    create_buffer(host_camera_lights_uniform_buffer, camera_lights_data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    allocate_and_bind_to_memory_buffer(host_camera_lights_allocation, host_camera_lights_uniform_buffer, VMA_MEMORY_USAGE_CPU_ONLY);
    vmaMapMemory(vma_allocator, host_camera_lights_allocation, &host_camera_lights_data);

    std::vector<VkBuffer> device_buffers_to_allocate;
    std::vector<VkImage> device_images_to_allocate;

    // We create the device buffer that is going to hold camera and lights information
    create_buffer(device_camera_lights_uniform_buffer, camera_lights_data_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    device_buffers_to_allocate.push_back(device_camera_lights_uniform_buffer);

    // Iterator range that goes only through shadowed lights
    auto shadowed_lights_it_range = boost::make_iterator_range(this->lights_container.get<1>().upper_bound(0),
                                                               this->lights_container.get<1>().end());
    std::vector<VkExtent2D> shadow_map_resolutions(boost::size(shadowed_lights_it_range));
    std::vector<uint32_t> ssbo_indices(boost::size(shadowed_lights_it_range));

    uint32_t j = 0;
    for (const auto& [shadowed_light, shadow_map_res, ssbo_index] : boost::combine(shadowed_lights_it_range, shadow_map_resolutions, ssbo_indices)) {
        shadow_map_res = {shadowed_light.get_shadow_map_resolution().x, shadowed_light.get_shadow_map_resolution().y};
        shadowed_light.light_params.shadow_map_index = j;
        ssbo_index = lights_container.iterator_to(shadowed_light) - lights_container.begin();
        j++;
    }

    vsm_context.create_resources(shadow_map_resolutions, ssbo_indices, pbr_model_data_set_layout, light_data_set_layout);
    smaa_context.create_resources(rendering_resolution, "resources//shaders");
    hbao_context.create_resources(rendering_resolution);
    hdr_tonemap_context.create_resources(rendering_resolution, "resources//shaders");
    if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		amd_fsr->create_resources(rendering_resolution, swapchain_create_info.imageExtent);
    }

    // We create some attachments useful during rendering
    create_image(device_depth_image, VK_FORMAT_D32_SFLOAT, {rendering_resolution.width, rendering_resolution.height, 1},
                 1, 1,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    device_images_to_allocate.push_back(device_depth_image);

    create_image(device_render_target, VK_FORMAT_B10G11R11_UFLOAT_PACK32, {rendering_resolution.width, rendering_resolution.height, 1},
                 2, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    device_images_to_allocate.push_back(device_render_target);

    create_image(device_normal_g_image, VK_FORMAT_R8G8B8A8_UNORM, {rendering_resolution.width, rendering_resolution.height, 1},
                 1, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    device_images_to_allocate.push_back(device_normal_g_image);

    create_image(device_global_ao_image, VK_FORMAT_R8_UNORM, {rendering_resolution.width, rendering_resolution.height, 1},
                 1, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    device_images_to_allocate.push_back(device_global_ao_image);

	create_image(device_tonemapped_image, VK_FORMAT_R8G8B8A8_UNORM, {rendering_resolution.width, rendering_resolution.height, 1},
			1, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	device_images_to_allocate.push_back(device_tonemapped_image);

	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		create_image(device_upscaled_image, VK_FORMAT_R8G8B8A8_UNORM, {swapchain_create_info.imageExtent.width, swapchain_create_info.imageExtent.height, 1},
				1, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		device_images_to_allocate.push_back(device_upscaled_image);
	}

    // We request information about the buffer and images we need from the contexts and store them in a vector
    auto vec = vsm_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), vec.begin(), vec.end());

    auto smaa_array_images = smaa_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), smaa_array_images.begin(), smaa_array_images.end());

    auto hbao_array_images = hbao_context.get_device_images();
    device_images_to_allocate.insert(device_images_to_allocate.end(), hbao_array_images.begin(), hbao_array_images.end());

	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		device_images_to_allocate.push_back(amd_fsr->get_device_image());
	}

    // We then allocate all needed images and buffers in the gpu, freeing the others first
	std::vector<VmaAllocation> out_allocations = { device_camera_lights_uniform_allocation};
	out_allocations.insert(out_allocations.begin(), device_attachments_allocations.begin(), device_attachments_allocations.end());
    allocate_and_bind_to_memory(out_allocations, device_buffers_to_allocate, device_images_to_allocate, VMA_MEMORY_USAGE_GPU_ONLY);
	device_camera_lights_uniform_allocation = out_allocations[0];
    device_attachments_allocations = std::vector<VmaAllocation>(out_allocations.begin() + 1, out_allocations.end());

    // We then create the image views
    create_image_view(device_depth_image_view, device_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0,1);
    create_image_view(device_render_target_image_views[0], device_render_target, VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
    create_image_view(device_render_target_image_views[1], device_render_target, VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    create_image_view(device_normal_g_image_view, device_normal_g_image, VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
    create_image_view(device_global_ao_image_view, device_global_ao_image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	create_image_view(device_tonemapped_image_view, device_tonemapped_image, VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		create_image_view(device_upscaled_image_view, device_upscaled_image, VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
	}

    vsm_context.init_resources();
    smaa_context.init_resources("resources//textures", physical_device_memory_properties, device_render_target_image_views[1], command_pool, command_buffers[0], queue);
    pbr_context.set_output_images(rendering_resolution, device_depth_image_view, device_render_target_image_views[0], device_normal_g_image_view);
    hbao_context.init_resources();
    hbao_context.update_constants(camera.get_proj_matrix());

    // After creating all resources we proceed to create the descriptor sets
    write_descriptor_sets();
    record_command_buffers();
}

void GraphicsModuleVulkanApp::write_descriptor_sets() {
    // Then we get all the required descriptors and request a single pool
    uint64_t all_primitives_count = 0;
    for (uint64_t i = 0; i < vk_models.size(); i++) {
		all_primitives_count += vk_models[i].device_primitives_data_info.size();
    }

    // uniform buffer is for the camera, the storage buffer is for the lights
    std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> sets_elements_required = {
            {
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 + all_primitives_count},
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_SHADOWED_LIGHTS + all_primitives_count},
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
            },
            static_cast<uint32_t>(1 + 1 + all_primitives_count)
    };

    vulkan_helper::insert_or_sum(sets_elements_required, vsm_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, smaa_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, hbao_context.get_required_descriptor_pool_size_and_sets());
    vulkan_helper::insert_or_sum(sets_elements_required, hdr_tonemap_context.get_required_descriptor_pool_size_and_sets());
	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		vulkan_helper::insert_or_sum(sets_elements_required, amd_fsr->get_required_descriptor_pool_size_and_sets());
	}
    std::vector<VkDescriptorPoolSize> descriptor_pool_size = vulkan_helper::convert_map_to_vector(sets_elements_required.first);

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            sets_elements_required.second,
            static_cast<uint32_t>(descriptor_pool_size.size()),
            descriptor_pool_size.data()
    };
    vkDestroyDescriptorPool(device, attachments_descriptor_pool, nullptr);
    vkCreateDescriptorPool(device, &descriptor_pool_create_info, nullptr, &attachments_descriptor_pool);

    // Next we allocate the descriptors of all the contextes
    vsm_context.allocate_descriptor_sets(attachments_descriptor_pool);
    smaa_context.allocate_descriptor_sets(attachments_descriptor_pool, device_render_target_image_views[0]);
    hbao_context.allocate_descriptor_sets(attachments_descriptor_pool, device_depth_image_view, device_normal_g_image_view, device_global_ao_image_view);
    hdr_tonemap_context.allocate_descriptor_sets(attachments_descriptor_pool, device_render_target_image_views[1], device_global_ao_image_view, {device_tonemapped_image_view});
	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		amd_fsr->allocate_descriptor_sets(attachments_descriptor_pool, device_tonemapped_image_view, device_upscaled_image_view);
	}

    // then we allocate descriptor sets for camera, lights and objects
    std::vector<VkDescriptorSetLayout> layouts_of_sets;
    layouts_of_sets.push_back(camera_data_set_layout);
    layouts_of_sets.push_back(light_data_set_layout);
    layouts_of_sets.insert(layouts_of_sets.end(), all_primitives_count, pbr_model_data_set_layout);

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
    std::vector<VkWriteDescriptorSet> write_descriptor_set(1 + 2);

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
        lights_container.front().copy_data_to_ptr(nullptr) * lights_container.size()
    };

    auto shadowed_lights_it_range = boost::make_iterator_range(this->lights_container.get<1>().upper_bound(0),
                                                               this->lights_container.get<1>().end());
    std::vector<VkDescriptorImageInfo> light_descriptor_image_infos(boost::size(shadowed_lights_it_range));
    for (const auto& [light, light_descriptor_image_info] : boost::combine(shadowed_lights_it_range, light_descriptor_image_infos)) {
        light_descriptor_image_info = {
            max_aniso_linear_sampler,
            vsm_context.get_image_view(light.light_params.shadow_map_index),
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
    auto it = descriptor_sets.begin() + 2;
    for (uint32_t i = 0, offset = 0; i < vk_models.size(); i++) {
    	auto vk_model_write_descriptor_sets = vk_models[i].get_descriptor_writes({ it, vk_models[i].device_primitives_data_info.size() },
						device_model_uniform_buffer, offset,
						physical_device_properties.limits.minUniformBufferOffsetAlignment);
    	it += vk_models[i].device_primitives_data_info.size();
    	offset += vk_models[i].copy_uniform_data(nullptr);
    	write_descriptor_set.insert(write_descriptor_set.end(), vk_model_write_descriptor_sets.begin(), vk_model_write_descriptor_sets.end());
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);

    auto it2 = write_descriptor_set.begin() + 3;
    for (uint32_t i = 0; i < vk_models.size(); i++) {
    	vk_models[i].clean_descriptor_writes({ it2, 2 });
    	it2 += vk_models[i].device_primitives_data_info.size() * 2;
    }
}

void GraphicsModuleVulkanApp::record_command_buffers() {
    vkResetCommandPool(device, command_pool, 0);

    for (uint32_t i = 0; i < swapchain_images.size(); i++) {
        VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, nullptr };
        vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info);

        VkBufferCopy buffer_copy = { 0,0,vulkan_helper::get_aligned_memory_size(vk_models.front().copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment) * vk_models.size()};
        vkCmdCopyBuffer(command_buffers[i], host_model_uniform_buffer, device_model_uniform_buffer, 1, &buffer_copy);

        buffer_copy = {0,0, vulkan_helper::get_aligned_memory_size(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minStorageBufferOffsetAlignment) +
                            lights_container.front().copy_data_to_ptr(nullptr) * lights_container.size()};
        vkCmdCopyBuffer(command_buffers[i], host_camera_lights_uniform_buffer, device_camera_lights_uniform_buffer, 1, &buffer_copy);

        // Transitioning layout from write to shader read
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
        vsm_context.record_into_command_buffer(command_buffers[i], descriptor_sets[1], vk_models);
        pbr_context.record_into_command_buffer(command_buffers[i], descriptor_sets[0], descriptor_sets[1], vk_models);
        smaa_context.record_into_command_buffer(command_buffers[i]);

        // TODO: here remove the true and replace with attribute from the class
        hbao_context.record_into_command_buffer(command_buffers[i], rendering_resolution, camera.znear, camera.zfar, true);
        hdr_tonemap_context.record_into_command_buffer(command_buffers[i], 0, rendering_resolution);

		VkImage image_to_copy;
		if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
			amd_fsr->record_into_command_buffer(command_buffers[i], device_tonemapped_image, device_upscaled_image);
			image_to_copy = device_upscaled_image;
		}
		else {
			image_to_copy = device_tonemapped_image;
		}

		VkImageMemoryBarrier image_memory_barrier = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				nullptr,
				0,
				VK_ACCESS_MEMORY_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				swapchain_images[i],
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

		VkImageBlit image_blit = {
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			{{0,0,0}, {static_cast<int32_t>(swapchain_create_info.imageExtent.width), static_cast<int32_t>(swapchain_create_info.imageExtent.height), 1}},
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
			{{0,0,0}, {static_cast<int32_t>(swapchain_create_info.imageExtent.width), static_cast<int32_t>(swapchain_create_info.imageExtent.height), 1}},
		};
        vkCmdBlitImage(command_buffers[i], image_to_copy, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_NEAREST);

        image_memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		image_memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

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
            i<lights_container.size(); i++) {
            offset += lights_container[i].copy_data_to_ptr(static_cast<uint8_t*>(host_camera_lights_data) + offset);
        }

        for(uint32_t i = 0, offset = 0; i < vk_models.size(); i++) {
            vk_models[i].copy_uniform_data(static_cast<uint8_t*>(host_model_uniform_buffer_ptr) + offset);
            offset += vulkan_helper::get_aligned_memory_size(vk_models[i].copy_uniform_data(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment);
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
	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		rendering_resolution = amd_fsr->get_recommended_input_resolution(swapchain_create_info.imageExtent);
	}
	else {
		rendering_resolution = swapchain_create_info.imageExtent;
	}
    init_renderer();
    resize_callback(this);
}

GraphicsModuleVulkanApp::~GraphicsModuleVulkanApp() {
    vkDeviceWaitIdle(device);
    for (auto& semaphore : semaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    vkDestroySampler(device, max_aniso_linear_sampler, nullptr);

    // Model uniform related things freed
    vmaUnmapMemory(this->vma_allocator, host_model_uniform_allocation);
    vkDestroyBuffer(device, host_model_uniform_buffer, nullptr);
    vmaFreeMemory(this->vma_allocator, host_model_uniform_allocation);

    // Model related things freed
    vk_models.clear();

    vkDestroyBuffer(device, device_mesh_data_buffer, nullptr);
    vmaFreeMemory(this->vma_allocator, device_mesh_data_allocation);
    vkDestroyBuffer(device, device_model_uniform_buffer, nullptr);
    vmaFreeMemory(this->vma_allocator, device_model_uniform_allocation);

    // Camera and light uniform related things freed
    vmaUnmapMemory(this->vma_allocator, host_camera_lights_allocation);
    vkDestroyBuffer(device, host_camera_lights_uniform_buffer, nullptr);
    vmaFreeMemory(this->vma_allocator, host_camera_lights_allocation);

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

	vkDestroyImageView(device, device_tonemapped_image_view, nullptr);
	vkDestroyImage(device, device_tonemapped_image, nullptr);

	vkDestroyImageView(device, device_upscaled_image_view, nullptr);
	vkDestroyImage(device, device_upscaled_image, nullptr);

	vkDestroyBuffer(device, device_camera_lights_uniform_buffer, nullptr);
    for (const auto& allocation : device_attachments_allocations) {
        vmaFreeMemory(this->vma_allocator, allocation);
    }

    vkDestroyDescriptorSetLayout(device, pbr_model_data_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, light_data_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, camera_data_set_layout, nullptr);
    vkDestroyDescriptorPool(device, attachments_descriptor_pool, nullptr);

	vmaFreeMemory(vma_allocator, amd_fsr_uniform_allocation);
    vmaDestroyAllocator(vma_allocator);
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

void GraphicsModuleVulkanApp::allocate_and_bind_to_memory(std::vector<VmaAllocation> &out_allocations, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VmaMemoryUsage vma_memory_usage) {
    for (auto& allocation : out_allocations) {
		vmaFreeMemory(vma_allocator, allocation);
    }
	out_allocations.resize(buffers.size() + images.size());

    for (uint32_t i = 0; i < buffers.size(); i++) {
        this->allocate_and_bind_to_memory_buffer(out_allocations[i], buffers[i], vma_memory_usage);
    }

    for (uint32_t i = 0, j = buffers.size(); i < images.size(); i++, j++) {
        this->allocate_and_bind_to_memory_image(out_allocations[j], images[i], vma_memory_usage);
    }
}

void GraphicsModuleVulkanApp::allocate_and_bind_to_memory_buffer(VmaAllocation &out_allocation, VkBuffer buffer, VmaMemoryUsage vma_memory_usage) {
    VmaAllocationCreateInfo vma_allocation_create_info = {0};
    vma_allocation_create_info.usage = vma_memory_usage;
    check_error(vmaAllocateMemoryForBuffer(this->vma_allocator, buffer, &vma_allocation_create_info, &out_allocation, nullptr),
                vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    check_error(vmaBindBufferMemory(this->vma_allocator, out_allocation, buffer), vulkan_helper::Error::BIND_IMAGE_MEMORY_FAILED);

}

void GraphicsModuleVulkanApp::allocate_and_bind_to_memory_image(VmaAllocation &out_allocation, VkImage image, VmaMemoryUsage vma_memory_usage) {
    VmaAllocationCreateInfo vma_allocation_create_info = {0};
    vma_allocation_create_info.usage = vma_memory_usage;
    check_error(vmaAllocateMemoryForImage(this->vma_allocator, image, &vma_allocation_create_info, &out_allocation, nullptr),
                vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    check_error(vmaBindImageMemory(this->vma_allocator, out_allocation, image), vulkan_helper::Error::BIND_IMAGE_MEMORY_FAILED);
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
