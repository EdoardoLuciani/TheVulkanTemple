#include "hdr_tonemap.h"
#include <vector>
#include <utility>
#include <unordered_map>
#include "../vulkan_helper.h"
#include "../volk.h"

HDRTonemap::HDRTonemap(VkDevice device) {
    this->device = device;

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

    // We create the descriptor set layout for the compute shader
    std::array<VkDescriptorSetLayoutBinding,2> descriptor_set_layout_binding;
    descriptor_set_layout_binding[0] = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            &device_max_aniso_linear_sampler
    };
    descriptor_set_layout_binding[1] = {
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            &device_max_aniso_linear_sampler
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            descriptor_set_layout_binding.size(),
            descriptor_set_layout_binding.data()
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hdr_tonemap_set_layout), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);
}

HDRTonemap::~HDRTonemap() {
    vkDestroyPipelineLayout(device, hdr_tonemap_pipeline_layout, nullptr);
    vkDestroyPipeline(device, hdr_tonemap_pipeline, nullptr);

    vkDestroySampler(device, device_max_aniso_linear_sampler, nullptr);
    vkDestroyDescriptorSetLayout(device, hdr_tonemap_set_layout, nullptr);
}

void HDRTonemap::create_resources(std::string shader_dir_path, uint32_t out_images_count) {
    // We get the output images count in the resources method
    this->out_images_count = out_images_count;

    // We create the pipeline and pipeline layout of the compute shader
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//gaussian_blur_x.comp.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule compute_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &compute_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_infos = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_COMPUTE_BIT,
            compute_shader_module,
            "main",
            nullptr
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &hdr_tonemap_set_layout,
            0,
            nullptr
    };
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &hdr_tonemap_pipeline_layout);

    VkComputePipelineCreateInfo compute_pipeline_create_info = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_infos,
            hdr_tonemap_pipeline_layout,
            VK_NULL_HANDLE,
            -1
    };
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, nullptr, &hdr_tonemap_pipeline);

    vkDestroyShaderModule(device, compute_shader_module, nullptr);
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> HDRTonemap::get_required_descriptor_pool_size_and_sets() {
    return {
            { {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, out_images_count},
              {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, out_images_count}},
            out_images_count};
}

void HDRTonemap::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, std::vector<VkImageView> out_image_views) {
    std::vector<VkDescriptorSetLayout> layouts_of_sets(out_images_count, hdr_tonemap_set_layout);
    hdr_tonemap_descriptor_sets.resize(out_images_count);
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(layouts_of_sets.size()),
            layouts_of_sets.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, hdr_tonemap_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    std::vector<VkDescriptorImageInfo> descriptor_image_infos(2*out_images_count);
    for(int i=0; i<2*descriptor_image_infos.size(); i++) {
        descriptor_image_infos[i] = {
                device_max_aniso_linear_sampler,
                input_image_view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        descriptor_image_infos[++i] = {
                device_max_aniso_linear_sampler,
                out_image_views[i/2],
                VK_IMAGE_LAYOUT_GENERAL
        };
    }

    std::vector<VkWriteDescriptorSet> write_descriptor_set(2*out_images_count);
    for(int i=0; i<2*write_descriptor_set.size(); i++) {
        write_descriptor_set[i] = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                hdr_tonemap_descriptor_sets[i/2],
                0,
                0,
                1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                &descriptor_image_infos[i],
                nullptr,
                nullptr
        };
        write_descriptor_set[++i] = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                nullptr,
                hdr_tonemap_descriptor_sets[i/2],
                1,
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                &descriptor_image_infos[i],
                nullptr,
                nullptr
        };
    }
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}
