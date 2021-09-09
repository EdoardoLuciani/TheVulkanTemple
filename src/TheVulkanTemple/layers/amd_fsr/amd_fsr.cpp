#include "amd_fsr.h"
#include "ffx_a.h"
#include <glm/glm.hpp>

#define A_CPU 1
#include "ffx_a.h"
#include "ffx_fsr1.h"

AmdFsr::AmdFsr(VkDevice device, Settings fsr_settings, std::string shader_dir_path) {
    this->device = device;
    this->fsr_settings = fsr_settings;

    VkSamplerCreateInfo sampler_create_info = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_ALWAYS,
        -1000.0f,
        1000.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &common_fsr_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    // Descriptor set layout common for all fsr shaders
    std::array<VkDescriptorSetLayoutBinding, 4> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
    };
    descriptor_set_layout_binding[2] = {
            2,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
    };
    descriptor_set_layout_binding[3] = {
            3,
            VK_DESCRIPTOR_TYPE_SAMPLER,
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
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &common_fsr_descriptor_set_layout), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    create_pipelines(shader_dir_path);

    // Creation for the device buffer that holds the fsr constants
    VkBufferCreateInfo buffer_create_info {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        sizeof(glm::uvec4)*5,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &device_fsr_constants_buffer), vulkan_helper::Error::BUFFER_CREATION_FAILED);
}

VkExtent2D AmdFsr::get_recommended_input_resolution(VkExtent2D display_image_size) {
	float scaling_factor = scaling_factors[static_cast<uint32_t>(fsr_settings.preset)];
	return {static_cast<uint32_t>(std::ceil(display_image_size.width * scaling_factor)),
			static_cast<uint32_t>(std::ceil(display_image_size.height * scaling_factor))};
}

float AmdFsr::get_negative_mip_bias() {
	return negative_mip_biases[static_cast<uint32_t>(fsr_settings.preset)];
}

void AmdFsr::create_pipelines(std::string shader_dir_path) {
    std::string shader_precision_postfix;
    if (fsr_settings.precision == Precision::FP32) {
        shader_precision_postfix = "_float.comp.spv";
    }
    else {
        shader_precision_postfix = "_half.comp.spv";
    }

    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//fsr_easu" + shader_precision_postfix, shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    std::array<VkShaderModule,2> shader_modules;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_modules[0]), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//fsr_rcas" + shader_precision_postfix, shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_modules[1]), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    std::array<VkPipelineShaderStageCreateInfo,2> pipeline_shaders_stage_create_infos {{
        {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0,
                VK_SHADER_STAGE_COMPUTE_BIT,
                shader_modules[0],
                "main",
                nullptr
        },
        {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0,
                VK_SHADER_STAGE_COMPUTE_BIT,
                shader_modules[1],
                "main",
                nullptr
        }
    }};

    std::array<VkDescriptorSetLayout,1> descriptor_set_layouts = {common_fsr_descriptor_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layouts.size(),
            descriptor_set_layouts.data(),
            0,
            nullptr
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &common_fsr_pipeline_layout);

    std::array<VkComputePipelineCreateInfo,2> compute_pipeline_create_infos {{
    {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_infos[0],
            common_fsr_pipeline_layout,
            VK_NULL_HANDLE,
            -1
    },
    {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_infos[1],
            common_fsr_pipeline_layout,
            VK_NULL_HANDLE,
            -1
    }
    }};
    vkCreateComputePipelines(device, VK_NULL_HANDLE, compute_pipeline_create_infos.size(), compute_pipeline_create_infos.data(), nullptr, fsr_pipelines.data());

    for (auto& shader_module : shader_modules) {
        vkDestroyShaderModule(device, shader_module, nullptr);
    }
}

void AmdFsr::create_resources(VkExtent2D input_image_size, VkExtent2D output_image_size) {
	this->input_image_size = input_image_size;
	this->output_image_size = output_image_size;

    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM,
            {output_image_size.width, output_image_size.height, 1},
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED
    };
    vkDestroyImage(device, device_out_easu_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_out_easu_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // Updating the constants based on the resolutions and sharpness
	FsrEasuCon(reinterpret_cast<AU1*>(&fsr_constants[0]), reinterpret_cast<AU1*>(&fsr_constants[1]),
			reinterpret_cast<AU1*>(&fsr_constants[2]), reinterpret_cast<AU1*>(&fsr_constants[3]),
			static_cast<AF1>(input_image_size.width), static_cast<AF1>(input_image_size.height),
			static_cast<AF1>(input_image_size.width), static_cast<AF1>(input_image_size.height),
			(AF1)output_image_size.width, (AF1)output_image_size.height);

	this->set_rcas_sharpness(0.2);

	// Setting the dispatch size for the compute shader
	dispatch_size.x = (output_image_size.width + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	dispatch_size.y = (output_image_size.height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	dispatch_size.z = 1;
}

void AmdFsr::set_rcas_sharpness(float sharpness) {
	FsrRcasCon(reinterpret_cast<AU1*>(&fsr_constants[4]), sharpness);
	fsr_constants_changed = true;
}

void AmdFsr::create_image_views() {
    VkImageViewCreateInfo image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            device_out_easu_image,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
    };
    vkDestroyImageView(device, device_out_easu_image_view, nullptr);
    check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &device_out_easu_image_view), vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> AmdFsr::get_required_descriptor_pool_size_and_sets() {
	return {
		{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
			{VK_DESCRIPTOR_TYPE_SAMPLER, 2},
		},
			2
	};
}

void AmdFsr::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView out_image_view) {
    create_image_views();

    std::array<VkDescriptorSetLayout, 2> descriptor_sets_to_allocate;
    descriptor_sets_to_allocate.fill(common_fsr_descriptor_set_layout);
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(descriptor_sets_to_allocate.size()),
            descriptor_sets_to_allocate.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, fsr_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    // Buffer that holds the FSR constants
    VkDescriptorBufferInfo descriptor_buffer_info = {
			device_fsr_constants_buffer,
			0,
			sizeof(glm::uvec4)*5
    };
	std::array<VkDescriptorImageInfo, 2> in_out_easu_descriptor_image_info;
    in_out_easu_descriptor_image_info[0] = {
        VK_NULL_HANDLE,
        input_image_view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    in_out_easu_descriptor_image_info[1] = {
        VK_NULL_HANDLE,
        device_out_easu_image_view,
        VK_IMAGE_LAYOUT_GENERAL
    };

	std::array<VkDescriptorImageInfo, 2> in_out_rcas_descriptor_image_info;
	in_out_rcas_descriptor_image_info[0] = {
		VK_NULL_HANDLE,
		device_out_easu_image_view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
    in_out_rcas_descriptor_image_info[1] = {
    	VK_NULL_HANDLE,
    	out_image_view,
    	VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorImageInfo sampler_descriptor = {
        common_fsr_sampler,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    std::array<VkWriteDescriptorSet,8> write_descriptor_set;
    write_descriptor_set[0] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            fsr_descriptor_sets[0],
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            nullptr,
            &descriptor_buffer_info,
            nullptr
    };
    write_descriptor_set[1] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            fsr_descriptor_sets[0],
            1,
            0,
            1,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            &in_out_easu_descriptor_image_info[0],
            nullptr,
            nullptr
    };
    write_descriptor_set[2] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            fsr_descriptor_sets[0],
            2,
            0,
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &in_out_easu_descriptor_image_info[1],
            nullptr,
            nullptr
    };
    write_descriptor_set[3] = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            fsr_descriptor_sets[0],
            3,
            0,
            1,
            VK_DESCRIPTOR_TYPE_SAMPLER,
            &sampler_descriptor,
            nullptr,
            nullptr
    };

    write_descriptor_set[4] = write_descriptor_set[0];
	write_descriptor_set[4].dstSet = fsr_descriptor_sets[1];

	write_descriptor_set[5] = write_descriptor_set[1];
	write_descriptor_set[5].dstSet = fsr_descriptor_sets[1];
	write_descriptor_set[5].pImageInfo = &in_out_rcas_descriptor_image_info[0];

	write_descriptor_set[6] = write_descriptor_set[2];
	write_descriptor_set[6].dstSet = fsr_descriptor_sets[1];
	write_descriptor_set[6].pImageInfo = &in_out_rcas_descriptor_image_info[1];

	write_descriptor_set[7] = write_descriptor_set[3];
	write_descriptor_set[7].dstSet = fsr_descriptor_sets[1];

	vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}

void AmdFsr::record_into_command_buffer(VkCommandBuffer command_buffer, VkImage input_image, VkImage output_image) {
	// todo: use push contants for the fsr_constants
	if (fsr_constants_changed) {
		VkBufferMemoryBarrier buffer_memory_barrier = {
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				nullptr,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				device_fsr_constants_buffer,
				0,
				VK_WHOLE_SIZE
		};
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);

		vkCmdUpdateBuffer(command_buffer, device_fsr_constants_buffer, 0, sizeof(fsr_constants), fsr_constants.data());

		buffer_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		buffer_memory_barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;

		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);

		fsr_constants_changed = false;
	}

	// EASU pass
	std::array<VkImageMemoryBarrier,2> image_memory_barriers;
	image_memory_barriers[0] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		input_image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	image_memory_barriers[1] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_out_easu_image,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, fsr_pipelines[0]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, common_fsr_pipeline_layout, 0, 1, &fsr_descriptor_sets[0], 0, nullptr);
	vkCmdDispatch(command_buffer, dispatch_size.x, dispatch_size.y, dispatch_size.z);

	// RCAS pass
	image_memory_barriers[0] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			device_out_easu_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	image_memory_barriers[1] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			output_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, fsr_pipelines[1]);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, common_fsr_pipeline_layout, 0, 1, &fsr_descriptor_sets[1], 0, nullptr);
	vkCmdDispatch(command_buffer, dispatch_size.x, dispatch_size.y, dispatch_size.z);

	image_memory_barriers[0] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			output_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	};
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());
}

AmdFsr::~AmdFsr() {
	vkDestroySampler(device, common_fsr_sampler, nullptr);
	vkDestroyDescriptorSetLayout(device, common_fsr_descriptor_set_layout, nullptr);
	vkDestroyBuffer(device, device_fsr_constants_buffer, nullptr);
	vkDestroyPipelineLayout(device, common_fsr_pipeline_layout, nullptr);
	for (auto& pipeline : fsr_pipelines) {
		vkDestroyPipeline(device, pipeline, nullptr);
	}
	vkDestroyImageView(device, device_out_easu_image_view, nullptr);
	vkDestroyImage(device, device_out_easu_image, nullptr);
}