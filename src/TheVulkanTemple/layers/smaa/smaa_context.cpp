#include "smaa_context.h"
#include <string>
#include "../../external/volk.h"
#include "../../vulkan_helper.h"

SmaaContext::SmaaContext(VkDevice device, VkFormat out_image_format, std::string shader_dir_path,
                         std::string resource_images_dir_path, const VkPhysicalDeviceMemoryProperties &memory_properties) {
    this->device = device;

    // Sampler we will use with the smaa images
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
    check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &device_render_target_sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);

    std::array<VkSampler, 3> immutable_samplers;
    immutable_samplers.fill(device_render_target_sampler);

    // We also need to create the layouts for the descriptors
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            immutable_samplers.data()
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &descriptor_set_layout_binding
    };
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[0]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Layout for the second descriptor for the second smaa pipeline
    descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            3,
            VK_SHADER_STAGE_VERTEX_BIT |VK_SHADER_STAGE_FRAGMENT_BIT,
            immutable_samplers.data()
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[1]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Layout for the third descriptor for the third smaa pipeline, same as the first
    descriptor_set_layout_binding = {
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            VK_SHADER_STAGE_VERTEX_BIT |VK_SHADER_STAGE_FRAGMENT_BIT,
            immutable_samplers.data()
    };
    descriptor_set_layout_create_info.pBindings = &descriptor_set_layout_binding;
    check_error(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[2]), vulkan_helper::Error::DESCRIPTOR_SET_LAYOUT_CREATION_FAILED);

    // Smaa edge renderpass
    std::array<VkAttachmentDescription, 2> attachment_description {{
    {
            0,
            VK_FORMAT_S8_UINT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    },
    {
            0,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    }}};

    std::array<VkAttachmentReference, 2> attachment_reference {{
            { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
            { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
    }};
    VkSubpassDescription subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &attachment_reference[1],
            nullptr,
            &attachment_reference[0],
            0,
            nullptr
    };

    VkRenderPassCreateInfo render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            attachment_description.size(),
            attachment_description.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_passes[0]);

    // Smaa weight renderpass
    attachment_description[0] = {
            0,
            VK_FORMAT_S8_UINT,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };
    attachment_description[1] = {
            0,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
    attachment_reference[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &attachment_reference[1],
            nullptr,
            &attachment_reference[0],
            0,
            nullptr
    };

    render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            attachment_description.size(),
            attachment_description.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_passes[1]);

    // Smaa blend renderpass creation
    attachment_description[0] = {
            0,
            out_image_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    subpass_description = {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &attachment_reference[0],
            nullptr,
            nullptr,
            0,
            nullptr
    };

    render_pass_create_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            1,
            attachment_description.data(),
            1,
            &subpass_description,
            0,
            nullptr
    };
    vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_passes[2]);

    // filling the structure that contains all the common pipeline structures
    pipeline_common_structures common_structures;
    common_structures.vertex_input = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            0,
            nullptr,
            0,
            nullptr
    };

    common_structures.input_assembly = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_FALSE
    };

    // random values for the viewport and scissors since the real values will be set dynamically
    common_structures.viewport = {
            0.0f,
            0.0f,
            800.0f,
            800.0f,
            0.0f,
            1.0f
    };
    common_structures.scissor = {
            {0,0},
            {800, 800}
    };
    common_structures.pipeline_viewport_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            0,
            1,
            &common_structures.viewport,
            1,
            &common_structures.scissor
    };

    common_structures.rasterization = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f
    };

    common_structures.multisample = {
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

    common_structures.color_blend_attachment_state = {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    common_structures.color_blend_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &common_structures.color_blend_attachment_state,
            {0.0f,0.0f,0.0f,0.0f}
    };

    // We set the viewport and scissor dynamically
    common_structures.dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };
    common_structures.pipeline_dynamic_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            common_structures.dynamic_states.size(),
            common_structures.dynamic_states.data()
    };

    create_edge_pipeline(common_structures, shader_dir_path);
    create_weight_pipeline(common_structures, shader_dir_path);
    create_blend_pipeline(common_structures, shader_dir_path);

    // reading the resources images from the disk and uploading them to a host buffer
    this->load_resource_images_to_host_memory(resource_images_dir_path, memory_properties);

    // Creating the static images for smaa
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            smaa_area_image_format,
            smaa_area_image_extent,
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

    image_create_info.format = smaa_search_image_format;
    image_create_info.extent = smaa_search_image_extent;
    vkDestroyImage(device, device_smaa_search_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_search_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
}

void SmaaContext::create_edge_pipeline(const pipeline_common_structures &common_structures, std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_edge.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_edge.frag.spv", shader_contents);
    shader_module_create_info.codeSize = shader_contents.size();
    shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
    VkShaderModule fragment_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED );

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

    VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_TRUE,
            {VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_GREATER, 0xff, 0xff, 1},
            {VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_GREATER, 0xff, 0xff, 1},
            0.0f,
            1.0f
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &smaa_descriptor_sets_layout[0],
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, smaa_pipelines_layout[0], nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_pipelines_layout[0]);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &common_structures.vertex_input,
            &common_structures.input_assembly,
            nullptr,
            &common_structures.pipeline_viewport_state_create_info,
            &common_structures.rasterization,
            &common_structures.multisample,
            &pipeline_depth_stencil_state_create_info,
            &common_structures.color_blend_state_create_info,
            &common_structures.pipeline_dynamic_state_create_info,
            smaa_pipelines_layout[0],
            render_passes[0],
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, smaa_pipelines[0], nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_pipelines[0]);
    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void SmaaContext::create_weight_pipeline(const pipeline_common_structures &common_structures, std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_weight.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_weight.frag.spv", shader_contents);
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

    VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_TRUE,
            {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xff, 0xff, 1 },
            {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xff, 0xff, 1 },
            0.0f,
            1.0f
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &smaa_descriptor_sets_layout[1],
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, smaa_pipelines_layout[1], nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_pipelines_layout[1]);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &common_structures.vertex_input,
            &common_structures.input_assembly,
            nullptr,
            &common_structures.pipeline_viewport_state_create_info,
            &common_structures.rasterization,
            &common_structures.multisample,
            &pipeline_depth_stencil_state_create_info,
            &common_structures.color_blend_state_create_info,
            &common_structures.pipeline_dynamic_state_create_info,
            smaa_pipelines_layout[1],
            render_passes[1],
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, smaa_pipelines[1], nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_pipelines[1]);

    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void SmaaContext::create_blend_pipeline(const pipeline_common_structures &common_structures, std::string shader_dir_path) {
    std::vector<uint8_t> shader_contents;
    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_blend.vert.spv", shader_contents);
    VkShaderModuleCreateInfo shader_module_create_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            shader_contents.size(),
            reinterpret_cast<uint32_t*>(shader_contents.data())
    };
    VkShaderModule vertex_shader_module;
    check_error(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module), vulkan_helper::Error::SHADER_MODULE_CREATION_FAILED);

    vulkan_helper::get_binary_file_content(shader_dir_path + "//smaa_blend.frag.spv", shader_contents);
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

    VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            {},
            {},
            0.0f,
            1.0f
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &smaa_descriptor_sets_layout[2],
            0,
            nullptr
    };
    vkDestroyPipelineLayout(device, smaa_pipelines_layout[2], nullptr);
    vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_pipelines_layout[2]);

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            pipeline_shaders_stage_create_info.size(),
            pipeline_shaders_stage_create_info.data(),
            &common_structures.vertex_input,
            &common_structures.input_assembly,
            nullptr,
            &common_structures.pipeline_viewport_state_create_info,
            &common_structures.rasterization,
            &common_structures.multisample,
            &pipeline_depth_stencil_state_create_info,
            &common_structures.color_blend_state_create_info,
            &common_structures.pipeline_dynamic_state_create_info,
            smaa_pipelines_layout[2],
            render_passes[2],
            0,
            VK_NULL_HANDLE,
            -1
    };
    vkDestroyPipeline(device, smaa_pipelines[2], nullptr);
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_pipelines[2]);

    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void SmaaContext::load_resource_images_to_host_memory(std::string resource_images_dir_path, const VkPhysicalDeviceMemoryProperties &memory_properties) {
    // We need to get the smaa resource images from disk and upload them to VRAM
    vulkan_helper::get_binary_file_content(resource_images_dir_path + "//AreaTexDX10.R8G8", area_tex_size, nullptr);
    vulkan_helper::get_binary_file_content(resource_images_dir_path + "//SearchTex.R8", search_tex_size, nullptr);

    // First we create the buffer and allocate it
    VkBufferCreateInfo buffer_create_info = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            area_tex_size + search_tex_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr
    };
    check_error(vkCreateBuffer(device, &buffer_create_info, nullptr, &host_resource_images_transition_buffer), vulkan_helper::Error::BUFFER_CREATION_FAILED);
    VkMemoryRequirements host_transition_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(device, host_resource_images_transition_buffer, &host_transition_buffer_memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            host_transition_buffer_memory_requirements.size,
            vulkan_helper::select_memory_index(memory_properties, host_transition_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    check_error(vkAllocateMemory(device, &memory_allocate_info, nullptr, &host_resource_images_transition_memory), vulkan_helper::Error::MEMORY_ALLOCATION_FAILED);
    check_error(vkBindBufferMemory(device, host_resource_images_transition_buffer, host_resource_images_transition_memory, 0), vulkan_helper::Error::BIND_BUFFER_MEMORY_FAILED);

    // We map the memory and copy the data
    void *dst_ptr;
    check_error(vkMapMemory(device, host_resource_images_transition_memory, 0, VK_WHOLE_SIZE, 0, &dst_ptr), vulkan_helper::Error::MEMORY_MAP_FAILED);
    vulkan_helper::get_binary_file_content(resource_images_dir_path + "//AreaTexDX10.R8G8", area_tex_size, dst_ptr);
    vulkan_helper::get_binary_file_content(resource_images_dir_path + "//SearchTex.R8", search_tex_size, static_cast<uint8_t*>(dst_ptr) + area_tex_size);
    vkUnmapMemory(device, host_resource_images_transition_memory);
}

std::array<VkImage, 2> SmaaContext::get_permanent_device_images() {
    return std::array<VkImage, 2>{device_smaa_area_image, device_smaa_search_image};
}

void SmaaContext::record_permanent_resources_copy_to_device_memory(VkCommandBuffer cb) {
    // We need to upload the smaa resource images from RAM to VRAM
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
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    image_memory_barriers[1] = image_memory_barriers[0];
    image_memory_barriers[1].image = device_smaa_search_image;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

    // We perform the copy from the host buffer to the device images
    VkBufferImageCopy buffer_image_copy = {
            0,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
            {0,0,0},
            smaa_area_image_extent
    };
    vkCmdCopyBufferToImage(cb, host_resource_images_transition_buffer, device_smaa_area_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
    buffer_image_copy = {
            area_tex_size,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
            {0,0,0},
            smaa_search_image_extent
    };
    vkCmdCopyBufferToImage(cb, host_resource_images_transition_buffer, device_smaa_search_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);


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
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    image_memory_barriers[1] = image_memory_barriers[0];
    image_memory_barriers[1].image = device_smaa_search_image;

    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());
}

void SmaaContext::clean_copy_resources() {
    vkDestroyBuffer(device, host_resource_images_transition_buffer, nullptr);
    vkFreeMemory(device, host_resource_images_transition_memory, nullptr);
}

SmaaContext::~SmaaContext() {
    vkDeviceWaitIdle(device);

    for (auto& smaa_descriptor_set_layout : smaa_descriptor_sets_layout) {
        vkDestroyDescriptorSetLayout(device, smaa_descriptor_set_layout, nullptr);
    }
    vkDestroySampler(device, device_render_target_sampler, nullptr);

    for (auto& render_pass : render_passes) {
        vkDestroyRenderPass(device, render_pass, nullptr);
    }

    vkDestroyImageView(device, device_smaa_area_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_search_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_stencil_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_data_edge_image_view, nullptr);
    vkDestroyImageView(device, device_smaa_data_weight_image_view, nullptr);

    vkDestroyImage(device, device_smaa_area_image, nullptr);
    vkDestroyImage(device, device_smaa_search_image, nullptr);
    vkDestroyImage(device, device_smaa_stencil_image, nullptr);
    vkDestroyImage(device, device_smaa_data_image, nullptr);

    for (auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for(auto& pipeline_layout : smaa_pipelines_layout) {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    }
    for(auto& pipeline : smaa_pipelines) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
}

std::array<VkImage, 2> SmaaContext::get_device_images() {
    return std::array<VkImage, 2>{device_smaa_stencil_image, device_smaa_data_image};
}

std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> SmaaContext::get_required_descriptor_pool_size_and_sets() {
    return {
            {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7}},
            3
    };
}

void SmaaContext::create_resources(VkExtent2D screen_res) {
    screen_extent = {screen_res.width, screen_res.height, 1};

    // Then we create the device images for the resources
    VkImageCreateInfo image_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_S8_UINT,
            screen_extent,
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED
    };
    // We create the images required for the smaa, first the stencil image
    vkDestroyImage(device, device_smaa_stencil_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_stencil_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);

    // Then the data_image which is gonna hold the edge and weight tex during rendering
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.arrayLayers = 2;
    image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkDestroyImage(device, device_smaa_data_image, nullptr);
    check_error(vkCreateImage(device, &image_create_info, nullptr, &device_smaa_data_image), vulkan_helper::Error::IMAGE_CREATION_FAILED);
}

void SmaaContext::create_image_views() {
    VkImageViewCreateInfo image_view_create_info;
    if (device_smaa_area_image_view == VK_NULL_HANDLE || device_smaa_search_image_view == VK_NULL_HANDLE) {
        image_view_create_info = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                nullptr,
                0,
                device_smaa_area_image,
                VK_IMAGE_VIEW_TYPE_2D,
                smaa_area_image_format,
                {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0 , 1 }
        };
        vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_area_image_view);

        image_view_create_info.image = device_smaa_search_image;
        image_view_create_info.format = smaa_search_image_format;
        vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_search_image_view);
    }

    image_view_create_info = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            device_smaa_stencil_image,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_S8_UINT,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            { VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0 , 1 }
    };
    vkDestroyImageView(device, device_smaa_stencil_image_view, nullptr);
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_stencil_image_view);

    image_view_create_info.image = device_smaa_data_image;
    image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkDestroyImageView(device, device_smaa_data_edge_image_view, nullptr);
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_data_edge_image_view);

    image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1, 1 };
    vkDestroyImageView(device, device_smaa_data_weight_image_view, nullptr);
    vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_data_weight_image_view);
}

void SmaaContext::create_framebuffers(VkImageView out_image_view) {
    std::array<VkImageView, 2> attachments = {
            device_smaa_stencil_image_view,
            device_smaa_data_edge_image_view
    };
    VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            render_passes[0],
            attachments.size(),
            attachments.data(),
            screen_extent.width,
            screen_extent.height,
            screen_extent.depth
    };
    vkDestroyFramebuffer(device, framebuffers[0], nullptr);
    vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[0]);

    attachments[0] = { device_smaa_stencil_image_view };
    attachments[1] = { device_smaa_data_weight_image_view };
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            render_passes[1],
            2,
            attachments.data(),
            screen_extent.width,
            screen_extent.height,
            screen_extent.depth
    };
    vkDestroyFramebuffer(device, framebuffers[1], nullptr);
    vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[1]);

    attachments[0] = { out_image_view };
    framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0,
            render_passes[2],
            1,
            attachments.data(),
            screen_extent.width,
            screen_extent.height,
            screen_extent.depth
    };
    vkDestroyFramebuffer(device, framebuffers[2], nullptr);
    vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[2]);
}

void SmaaContext::init_resources(VkImageView out_image_view) {
    // We know that the images have already been allocated so we create the views and the framebuffers
    create_image_views();
    create_framebuffers(out_image_view);
}

void SmaaContext::allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            descriptor_pool,
            static_cast<uint32_t>(smaa_descriptor_sets_layout.size()),
            smaa_descriptor_sets_layout.data()
    };
    check_error(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, smaa_descriptor_sets.data()), vulkan_helper::Error::DESCRIPTOR_SET_ALLOCATION_FAILED);

    std::array<VkDescriptorImageInfo, 1> smaa_edge_descriptor_images_info {{
            { device_render_target_sampler, input_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    }};

    std::array<VkDescriptorImageInfo, 3> smaa_weight_descriptor_images_info {{
            { device_render_target_sampler, device_smaa_data_edge_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, device_smaa_area_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, device_smaa_search_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    }};

    std::array<VkDescriptorImageInfo, 2> smaa_blend_descriptor_images_info {{
            { device_render_target_sampler, input_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
            { device_render_target_sampler, device_smaa_data_weight_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
    }};

    std::array<VkWriteDescriptorSet,3> write_descriptor_set {{{
        // First write is for the smaa edge descriptor
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[0],
        0,
        0,
        smaa_edge_descriptor_images_info.size(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        smaa_edge_descriptor_images_info.data(),
        nullptr,
        nullptr
        },
            // Second writes are for the smaa weight descriptor
        {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[1],
        0,
        0,
        smaa_weight_descriptor_images_info.size(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        smaa_weight_descriptor_images_info.data(),
        nullptr,
        nullptr
        },
            // Third write is for the smaa blend descriptor
        {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        smaa_descriptor_sets[2],
        0,
        0,
        smaa_blend_descriptor_images_info.size(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        smaa_blend_descriptor_images_info.data(),
        nullptr,
        nullptr
        }
    }};
    vkUpdateDescriptorSets(device, write_descriptor_set.size(), write_descriptor_set.data(), 0, nullptr);
}

void SmaaContext::record_into_command_buffer(VkCommandBuffer command_buffer) {
    std::array<VkClearValue , 2> clear_colors;
    clear_colors[0].depthStencil = { 1.0f, 0 };
    clear_colors[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };

    VkViewport viewport = {
            0.0f,
            0.0f,
            static_cast<float>(screen_extent.width),
            static_cast<float>(screen_extent.height),
            0.0f,
            1.0f
    };
    VkRect2D scissor = {
            {0,0},
            {screen_extent.width, screen_extent.height}
    };

    // Smaa edge renderpass start
    VkRenderPassBeginInfo render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            render_passes[0],
            framebuffers[0],
            {{0,0},{screen_extent.width, screen_extent.height}},
            clear_colors.size(),
            clear_colors.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines[0]);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines_layout[0], 0, 1, &smaa_descriptor_sets[0], 0, nullptr);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // Smaa weight renderpass start
    render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            render_passes[1],
            framebuffers[1],
            {{0,0},{screen_extent.width, screen_extent.height}},
            clear_colors.size(),
            clear_colors.data()
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines[1]);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines_layout[1], 0, 1, &smaa_descriptor_sets[1], 0, nullptr);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    // Smaa blend renderpass start
    render_pass_begin_info = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            render_passes[2],
            framebuffers[2],
            {{0,0},{screen_extent.width, screen_extent.height}},
            1,
            &clear_colors[1]
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines[2]);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_pipelines_layout[2], 0, 1, &smaa_descriptor_sets[2], 0, nullptr);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    // Smaa weight renderpass end
    vkCmdEndRenderPass(command_buffer);
}