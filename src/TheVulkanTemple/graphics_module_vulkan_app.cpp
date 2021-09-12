#include "graphics_module_vulkan_app.h"
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <span>
#include <thread>
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
						vma_wrapper(instance, selected_physical_device, device, vulkan_api_version, VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT, 512000000),
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
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &shadow_map_linear_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

	host_uniform_allocator = std::make_unique<VkBuffersBuddySubAllocator>(vma_wrapper.get_allocator(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU, 65536);

	device_mesh_and_index_allocator = std::make_unique<VkBuffersBuddySubAllocator>(vma_wrapper.get_allocator(),
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY, std::exp2(29)); // close to half a GB, precisely 536870912

    // We create 3 copies of frame data
    VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
    VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,nullptr,VK_FENCE_CREATE_SIGNALED_BIT };
    for (auto& frame : frames_data) {
    	vkCreateFence(device, &fence_create_info, nullptr, &frame.after_execution_fence);
    	frame.semaphores.resize(5);
    	for (uint32_t i = 0; i < frame.semaphores.size(); i++) {
    		vkCreateSemaphore(device, &semaphore_create_info, nullptr, &frame.semaphores[i]);
    	}
    	// Creating one pool for each thread
    	create_cmd_pool_and_buffers(main_queue_family_index, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, frame.vsm_command);
    	create_cmd_pool_and_buffers(main_queue_family_index, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, frame.pbr_command);
    	create_cmd_pool_and_buffers(main_queue_family_index, VK_COMMAND_BUFFER_LEVEL_PRIMARY, swapchain_images.size(), frame.swapchain_copy_static_commands);
    	create_cmd_pool_and_buffers(main_queue_family_index, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, frame.post_processing_static_command);
    }

    fence_create_info.flags = 0;
    vkCreateFence(device, &fence_create_info, nullptr, &memory_update_fence);

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
        instance_extensions.push_back("VK_KHR_win32_surface");
    #elif __linux__
        #ifdef VK_USE_PLATFORM_WAYLAND_KHR
            instance_extensions.push_back("VK_KHR_wayland_surface");
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

    // Creating the descriptor set layout for the camera
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
		auto infos = gltf_models.back().copy_model_data_in_ptr(GltfModel::v_model_attributes::V_ALL, true, true,
                                              GltfModel::t_model_attributes::T_ALL, nullptr, true);

		vk_models.emplace_back(VkModel(device, model_file_matrix[i].first, infos, model_file_matrix[i].second));
        models_total_size += vk_models.back().get_all_primitives_total_size();
    }
	VkBuffersBuddySubAllocator host_model_data_allocator(this->vma_wrapper.get_allocator(),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, models_total_size);

	// After getting a pointer for the host memory, we iterate once again in the models to copy them to memory and store some information about them
    std::vector<VkBuffersBuddySubAllocator::sub_allocation_data> host_models_sub_allocation_data;
	host_models_sub_allocation_data.resize(gltf_models.size());
	device_model_mesh_and_index_allocation_data.resize(gltf_models.size());
    for (uint32_t i=0; i < gltf_models.size(); i++) {
		host_models_sub_allocation_data[i] = host_model_data_allocator.suballocate(vk_models[i].get_all_primitives_total_size());
        gltf_models[i].copy_model_data_in_ptr(GltfModel::v_model_attributes::V_ALL, false, true,
                                                GltfModel::t_model_attributes::T_ALL, host_models_sub_allocation_data[i].allocation_host_ptr, false);
		// The mesh in the device buffer needs to be aligned to the attribute first size, which is always the position hence 12
		device_model_mesh_and_index_allocation_data[i] = device_mesh_and_index_allocator->suballocate(vk_models[i].get_all_primitives_mesh_and_indices_size(), 12);
    }

    // Suballocating the memory for the uniforms
	model_uniform_allocation_data.resize(vk_models.size());
	for (uint32_t i = 0; i < vk_models.size(); i++) {
		model_uniform_allocation_data[i] = host_uniform_allocator->suballocate(vk_models[i].copy_uniform_data(nullptr),
				physical_device_properties.limits.minUniformBufferOffsetAlignment);
	}

    for (uint32_t i = 0; i < vk_models.size(); i++) {
    	vk_models[i].vk_create_images(amd_fsr ? amd_fsr->get_negative_mip_bias() : 0.0f, vma_wrapper.get_allocator());
    }

    // After setting the required memory we register the command buffer to upload the data
    VkCommandBuffer temporary_command_buffer = frames_data.front().post_processing_static_command.command_buffers[0];
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        nullptr
    };
    vkBeginCommandBuffer(temporary_command_buffer, &command_buffer_begin_info);

	device_mesh_and_index_allocator->vk_record_buffers_pipeline_barrier(temporary_command_buffer, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // We copy the model data to the device buffers and images
	for (uint32_t i = 0; i < vk_models.size(); i++) {
        vk_models[i].vk_init_model(temporary_command_buffer, host_models_sub_allocation_data[i].buffer, host_models_sub_allocation_data[i].buffer_offset,
                                   device_model_mesh_and_index_allocation_data[i].buffer, device_model_mesh_and_index_allocation_data[i].buffer_offset);
	}

	device_mesh_and_index_allocator->vk_record_buffers_pipeline_barrier(temporary_command_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

	vkEndCommandBuffer(temporary_command_buffer);

    // After registering the command we submit the command_buffer with a generic fence
    submit_command_buffers({temporary_command_buffer}, VK_PIPELINE_STAGE_TRANSFER_BIT, {}, {}, memory_update_fence);
    vkWaitForFences(device, 1, &memory_update_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    // After the copy has finished we do cleanup
    vkResetCommandPool(device, frames_data.front().post_processing_static_command.command_pool, 0);
    vkResetFences(device, 1, &memory_update_fence);
	for (auto& sub_allocation_data : host_models_sub_allocation_data) {
		host_model_data_allocator.free(sub_allocation_data);
	}
}

void GraphicsModuleVulkanApp::load_lights(std::vector<Light> &&lights) {
    this->lights_container.assign(lights.begin(), lights.end());
}

void GraphicsModuleVulkanApp::set_camera(Camera &&camera) {
    this->camera = camera;
}

void GraphicsModuleVulkanApp::init_renderer() {
	// We check if the camera has been already allocated and if not we allocate it, while for the lights we free and reallocate directly
	if (camera_allocation_data.allocation_host_ptr == nullptr) {
		camera_allocation_data = host_uniform_allocator->suballocate(camera.copy_data_to_ptr(nullptr), physical_device_properties.limits.minUniformBufferOffsetAlignment);
	}

	if (lights_allocation_data.allocation_host_ptr != nullptr) {
		host_uniform_allocator->free(lights_allocation_data);
	}
	lights_allocation_data = host_uniform_allocator->suballocate(lights_container.front().copy_data_to_ptr(nullptr) * lights_container.size(),
                                                                 physical_device_properties.limits.minStorageBufferOffsetAlignment);

    std::vector<VkBuffer> device_buffers_to_allocate;
    std::vector<VkImage> device_images_to_allocate;

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
	std::vector<VmaAllocation> out_allocations = {};
	out_allocations.insert(out_allocations.begin(), device_attachments_allocations.begin(), device_attachments_allocations.end());
    allocate_and_bind_to_memory(out_allocations, device_buffers_to_allocate, device_images_to_allocate, VMA_MEMORY_USAGE_GPU_ONLY);
    device_attachments_allocations = std::vector<VmaAllocation>(out_allocations.begin(), out_allocations.end());

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
	smaa_context.init_resources("resources//textures", physical_device_memory_properties, device_render_target_image_views[1],
			frames_data.front().post_processing_static_command.command_pool, frames_data.front().post_processing_static_command.command_buffers[0], queue);
    pbr_context.set_output_images(rendering_resolution, device_depth_image_view, device_render_target_image_views[0], device_normal_g_image_view);
    hbao_context.init_resources();
    hbao_context.update_constants(camera.get_proj_matrix());

    // After creating all resources we proceed to create the descriptor sets
    write_descriptor_sets();
    for (auto& frame : frames_data) {
    	record_static_command_buffers(frame.post_processing_static_command, frame.swapchain_copy_static_commands);
    }
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
            camera_allocation_data.buffer,
			camera_allocation_data.buffer_offset,
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
			lights_allocation_data.buffer,
			lights_allocation_data.buffer_offset,
			lights_container.front().copy_data_to_ptr(nullptr) * lights_container.size()
	};

    auto shadowed_lights_it_range = boost::make_iterator_range(this->lights_container.get<1>().upper_bound(0),
                                                               this->lights_container.get<1>().end());

    std::vector<VkDescriptorImageInfo> light_descriptor_image_infos(boost::size(shadowed_lights_it_range));
    for (const auto& [light, light_descriptor_image_info] : boost::combine(shadowed_lights_it_range, light_descriptor_image_infos)) {
        light_descriptor_image_info = {
				shadow_map_linear_sampler,
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
    for (uint32_t i = 0; i < vk_models.size(); i++) {
    	auto vk_model_write_descriptor_sets = vk_models[i].get_descriptor_writes({ it, vk_models[i].device_primitives_data_info.size() },
						model_uniform_allocation_data[i].buffer, model_uniform_allocation_data[i].buffer_offset);
    	it += vk_models[i].device_primitives_data_info.size();
    	write_descriptor_set.insert(write_descriptor_set.end(), vk_model_write_descriptor_sets.begin(), vk_model_write_descriptor_sets.end());
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);

    auto it2 = write_descriptor_set.begin() + 3;
    for (uint32_t i = 0; i < vk_models.size(); i++) {
    	vk_models[i].clean_descriptor_writes({ it2, 2 });
    	it2 += vk_models[i].device_primitives_data_info.size() * 2;
    }
}

void GraphicsModuleVulkanApp::record_static_command_buffers(command_record_info post_processing, command_record_info swapchain_copy_commands) {
	vkResetCommandPool(device, post_processing.command_pool, 0);
	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};

	// command buffer for recording image post-processing
	vkBeginCommandBuffer(post_processing.command_buffers[0], &command_buffer_begin_info);
	smaa_context.record_into_command_buffer(post_processing.command_buffers[0]);
	hbao_context.record_into_command_buffer(post_processing.command_buffers[0], rendering_resolution, camera.get_znear(), camera.get_zfar(), true);
	hdr_tonemap_context.record_into_command_buffer(post_processing.command_buffers[0], 0, rendering_resolution);
	if (engine_options.fsr_settings.preset != AmdFsr::Preset::NONE) {
		amd_fsr->record_into_command_buffer(post_processing.command_buffers[0], device_tonemapped_image, device_upscaled_image);
	}
	vkEndCommandBuffer(post_processing.command_buffers[0]);

	// command buffers for each swapchain image, in which it registers the copy
	vkResetCommandPool(device, swapchain_copy_commands.command_pool, 0);
	for(uint32_t i = 0; i < swapchain_copy_commands.command_buffers.size(); i++) {
		vkBeginCommandBuffer(swapchain_copy_commands.command_buffers[i], &command_buffer_begin_info);

		VkImage image_to_copy = amd_fsr ? device_upscaled_image : device_tonemapped_image;
		// Copying output image to the swapchain one
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
		vkCmdPipelineBarrier(swapchain_copy_commands.command_buffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

		VkImageBlit image_blit = {
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{{0,0,0}, {static_cast<int32_t>(swapchain_create_info.imageExtent.width), static_cast<int32_t>(swapchain_create_info.imageExtent.height), 1}},
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
				{{0,0,0}, {static_cast<int32_t>(swapchain_create_info.imageExtent.width), static_cast<int32_t>(swapchain_create_info.imageExtent.height), 1}},
				};
		vkCmdBlitImage(swapchain_copy_commands.command_buffers[i], image_to_copy, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain_images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_NEAREST);

		image_memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		image_memory_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(swapchain_copy_commands.command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
		vkEndCommandBuffer(swapchain_copy_commands.command_buffers[i]);
	}
}

void GraphicsModuleVulkanApp::record_vsm_command_buffer(command_record_info vsm_to_record) {
	// command buffer for the vsm draw commands
	vkResetCommandPool(device, vsm_to_record.command_pool, 0);
	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
	vkBeginCommandBuffer(vsm_to_record.command_buffers[0], &command_buffer_begin_info);
	std::vector<VkDescriptorSet> object_descriptor_sets(descriptor_sets.begin() + 2, descriptor_sets.end());
	vsm_context.record_into_command_buffer(vsm_to_record.command_buffers[0], descriptor_sets[1], vk_models);
	vkEndCommandBuffer(vsm_to_record.command_buffers[0]);
}

void GraphicsModuleVulkanApp::record_pbr_command_buffer(command_record_info pbr_to_record) {
	// command buffer for the pbr draw commands
	vkResetCommandPool(device, pbr_to_record.command_pool, 0);
	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
	vkBeginCommandBuffer(pbr_to_record.command_buffers[0], &command_buffer_begin_info);
	pbr_context.record_into_command_buffer(pbr_to_record.command_buffers[0], descriptor_sets[0], descriptor_sets[1], vk_models, camera);
	vkEndCommandBuffer(pbr_to_record.command_buffers[0]);
}

void GraphicsModuleVulkanApp::start_frame_loop(std::function<void(GraphicsModuleVulkanApp*)> resize_callback,
                                               std::function<void(GraphicsModuleVulkanApp*, uint32_t)> pre_submit_callback) {
    uint32_t rendered_frames = 0;
    uint64_t all_rendered_frames = 0;
    std::chrono::steady_clock::time_point frames_time, current_frame, last_frame;
    uint32_t delta_time;

    frame_data* current_frame_data = &frames_data[all_rendered_frames % frames_data.size()];
    frame_data* next_frame_data = &frames_data[(all_rendered_frames + 1) % frames_data.size()];
    vkResetFences(device, 1, &current_frame_data->after_execution_fence);
    std::thread vsm_record_thread = std::thread(&GraphicsModuleVulkanApp::record_vsm_command_buffer, this, current_frame_data->vsm_command);
    std::thread pbr_record_thread = std::thread(&GraphicsModuleVulkanApp::record_pbr_command_buffer, this, current_frame_data->pbr_command);

    auto resize_lambda = [&](frame_data *frame_data_to_record) {
    	on_window_resize(resize_callback);
    	vsm_record_thread.join();
    	pbr_record_thread.join();
    	vsm_record_thread = std::thread(&GraphicsModuleVulkanApp::record_vsm_command_buffer, this, frame_data_to_record->vsm_command);
    	pbr_record_thread = std::thread(&GraphicsModuleVulkanApp::record_pbr_command_buffer, this, frame_data_to_record->pbr_command);
    };

    while (!glfwWindowShouldClose(window)) {
    	// Pre submit work
        current_frame = std::chrono::steady_clock::now();
        delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_frame - last_frame).count();
        last_frame = current_frame;

        glfwPollEvents();
        pre_submit_callback(this, delta_time);

        camera.copy_data_to_ptr(static_cast<uint8_t*>(camera_allocation_data.allocation_host_ptr));
        for(uint32_t i = 0, offset = 0; i < lights_container.size(); i++) {
            offset += lights_container[i].copy_data_to_ptr(static_cast<uint8_t*>(lights_allocation_data.allocation_host_ptr) + offset);
        }

        for(uint32_t i = 0; i < vk_models.size(); i++) {
            vk_models[i].copy_uniform_data(static_cast<uint8_t*>(model_uniform_allocation_data[i].allocation_host_ptr));
        }

        // Start of frame submission
        current_frame_data = &frames_data[all_rendered_frames % frames_data.size()];
        next_frame_data = &frames_data[(all_rendered_frames + 1) % frames_data.size()];

        uint32_t image_index = 0;
        VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, current_frame_data->semaphores[0], VK_NULL_HANDLE, &image_index);
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
			/* TODO: if vkAcquireNextImageKHR fails, current_frame_data->semaphores[0] is still signaled so when the loop resets,
			vkAcquireNextImageKHR will be met by a signaled semaphore and vulkan will complain. Either solve this with a timeline semaphore or
			just ignore this until it becomes a serious issue. */
            resize_lambda(current_frame_data);
            continue;
        }
        else if (res != VK_SUCCESS) {
            check_error(res, vulkan_helper::Error::ACQUIRE_NEXT_IMAGE_FAILED);
        }

        std::array<VkSubmitInfo, 4> submit_infos;
        submit_infos[0] = {
        		VK_STRUCTURE_TYPE_SUBMIT_INFO,
        		nullptr,
        		0,
        		nullptr,
        		nullptr,
        		1,
        		&current_frame_data->vsm_command.command_buffers[0],
        		1,
        		&current_frame_data->semaphores[1]
        };
        VkPipelineStageFlags fragment_flag = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        submit_infos[1] = {
        		VK_STRUCTURE_TYPE_SUBMIT_INFO,
        		nullptr,
        		1,
        		&current_frame_data->semaphores[1],
        		&fragment_flag,
        		1,
        		&current_frame_data->pbr_command.command_buffers[0],
        		1,
        		&current_frame_data->semaphores[2]
        };
        VkPipelineStageFlags color_attachment_stage_flag = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit_infos[2] = {
        		VK_STRUCTURE_TYPE_SUBMIT_INFO,
        		nullptr,
        		1,
        		&current_frame_data->semaphores[2],
        		&color_attachment_stage_flag,
        		1,
        		&current_frame_data->post_processing_static_command.command_buffers[0],
        		1,
        		&current_frame_data->semaphores[3]
        };
        std::array<VkPipelineStageFlags, 2> stage_flags = {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT};
        std::array<VkSemaphore, 2> semaphores_to_wait = {current_frame_data->semaphores[3], current_frame_data->semaphores[0]};
        submit_infos[3] = {
        		VK_STRUCTURE_TYPE_SUBMIT_INFO,
        		nullptr,
        		semaphores_to_wait.size(),
        		semaphores_to_wait.data(),
        		stage_flags.data(),
        		1,
        		&current_frame_data->swapchain_copy_static_commands.command_buffers[image_index],
        		1,
        		&current_frame_data->semaphores[4]
        };
        vsm_record_thread.join();
        pbr_record_thread.join();
        check_error(vkQueueSubmit(queue, submit_infos.size(), submit_infos.data(), current_frame_data->after_execution_fence), vulkan_helper::Error::QUEUE_SUBMIT_FAILED);

        // Start of current frame post-submit work for next frame
        vkWaitForFences(device, 1, &next_frame_data->after_execution_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
        vkResetFences(device, 1, &next_frame_data->after_execution_fence);
        vsm_record_thread = std::thread(&GraphicsModuleVulkanApp::record_vsm_command_buffer, this, next_frame_data->vsm_command);
        pbr_record_thread = std::thread(&GraphicsModuleVulkanApp::record_pbr_command_buffer, this, next_frame_data->pbr_command);

        // Start of frame present
        VkPresentInfoKHR present_info = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                nullptr,
                1,
                &current_frame_data->semaphores[4],
                1,
                &swapchain,
                &image_index,
                nullptr
        };
        res = vkQueuePresentKHR(queue, &present_info);
        rendered_frames++;
        all_rendered_frames++;
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
            resize_lambda(next_frame_data);
            continue;
        }
        else if (res != VK_SUCCESS) {
			check_error(res, vulkan_helper::Error::QUEUE_PRESENT_FAILED);
		}

        uint32_t time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - frames_time).count();
        if ( time_diff > 1000) {
            std::cout << "Msec/frame: " << ( time_diff / static_cast<float>(rendered_frames)) << std::endl;
            rendered_frames = 0;
            frames_time = std::chrono::steady_clock::now();
        }
    }
    // The thread need to be waited upon before exiting the function
    vsm_record_thread.join();
    pbr_record_thread.join();
}

void GraphicsModuleVulkanApp::on_window_resize(std::function<void(GraphicsModuleVulkanApp*)> resize_callback) {
    vkDeviceWaitIdle(device);
    create_swapchain();
    rendering_resolution = amd_fsr ? amd_fsr->get_recommended_input_resolution(swapchain_create_info.imageExtent) : swapchain_create_info.imageExtent;
    init_renderer();
    resize_callback(this);
}

GraphicsModuleVulkanApp::~GraphicsModuleVulkanApp() {
    vkDeviceWaitIdle(device);

    for (auto& frame : frames_data) {
    	for (auto & semaphore : frame.semaphores) {
    		vkDestroySemaphore(device, semaphore, nullptr);
    	}
    	vkDestroyFence(device, frame.after_execution_fence, nullptr);
        delete_cmd_pool_and_buffers(frame.swapchain_copy_static_commands);
        delete_cmd_pool_and_buffers(frame.post_processing_static_command);
        delete_cmd_pool_and_buffers(frame.vsm_command);
        delete_cmd_pool_and_buffers(frame.pbr_command);
    }
    vkDestroySampler(device, shadow_map_linear_sampler, nullptr);
	vkDestroyFence(device, memory_update_fence, nullptr);

	// camera and lights uniform freed
	host_uniform_allocator->free(camera_allocation_data);
	host_uniform_allocator->free(lights_allocation_data);

    // Model related things freed
    for (auto& allocation_data : model_uniform_allocation_data) {
		host_uniform_allocator->free(allocation_data);
	}
	for (auto& allocation_data : device_model_mesh_and_index_allocation_data) {
		device_mesh_and_index_allocator->free(allocation_data);
	}

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

    for (const auto& allocation : device_attachments_allocations) {
        vmaFreeMemory(this->vma_wrapper.get_allocator(), allocation);
    }

    vkDestroyDescriptorSetLayout(device, pbr_model_data_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, light_data_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, camera_data_set_layout, nullptr);
    vkDestroyDescriptorPool(device, attachments_descriptor_pool, nullptr);

	vmaFreeMemory(vma_wrapper.get_allocator(), amd_fsr_uniform_allocation);
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
		vmaFreeMemory(vma_wrapper.get_allocator(), allocation);
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
    check_error(vmaAllocateMemoryForBuffer(this->vma_wrapper.get_allocator(), buffer, &vma_allocation_create_info, &out_allocation, nullptr),
                vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    check_error(vmaBindBufferMemory(this->vma_wrapper.get_allocator(), out_allocation, buffer), vulkan_helper::Error::BIND_IMAGE_MEMORY_FAILED);

}

void GraphicsModuleVulkanApp::allocate_and_bind_to_memory_image(VmaAllocation &out_allocation, VkImage image, VmaMemoryUsage vma_memory_usage) {
    VmaAllocationCreateInfo vma_allocation_create_info = {0};
    vma_allocation_create_info.usage = vma_memory_usage;
    check_error(vmaAllocateMemoryForImage(this->vma_wrapper.get_allocator(), image, &vma_allocation_create_info, &out_allocation, nullptr),
                vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);

    check_error(vmaBindImageMemory(this->vma_wrapper.get_allocator(), out_allocation, image), vulkan_helper::Error::BIND_IMAGE_MEMORY_FAILED);
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