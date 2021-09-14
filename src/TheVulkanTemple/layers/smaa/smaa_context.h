#ifndef BASE_VULKAN_APP_SMAA_CONTEXT_H
#define BASE_VULKAN_APP_SMAA_CONTEXT_H
#include "../../external/volk.h"
#include <array>
#include <string>
#include <unordered_map>

class SmaaContext {
    public:
        SmaaContext(VkDevice device, VkFormat out_image_format, std::string shader_dir_path, std::string resource_images_dir_path, const VkPhysicalDeviceMemoryProperties &memory_properties);
        std::array<VkImage, 2> get_permanent_device_images();
        void record_permanent_resources_copy_to_device_memory(VkCommandBuffer cb);
        void clean_copy_resources();
        ~SmaaContext();

        std::array<VkImage, 2> get_device_images();
        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void create_resources(VkExtent2D screen_res);
        void init_resources(VkImageView out_image_view);

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view);

        void record_into_command_buffer(VkCommandBuffer command_buffer);

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkSampler device_render_target_sampler = VK_NULL_HANDLE;
        std::array<VkDescriptorSetLayout, 3> smaa_descriptor_sets_layout = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkRenderPass, 3> render_passes = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        struct pipeline_common_structures {
            VkPipelineVertexInputStateCreateInfo vertex_input;
            VkPipelineInputAssemblyStateCreateInfo input_assembly;
            VkViewport viewport;
            VkRect2D scissor;
            VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info;
            VkPipelineRasterizationStateCreateInfo rasterization;
            VkPipelineMultisampleStateCreateInfo multisample;
            VkPipelineColorBlendAttachmentState color_blend_attachment_state;
            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info;
            std::array<VkDynamicState,2> dynamic_states;
            VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info;
        };
        VkExtent3D screen_extent;

        uint64_t area_tex_size, search_tex_size;
        VkBuffer host_resource_images_transition_buffer;
        VkDeviceMemory host_resource_images_transition_memory;

        // Static images we load from the disk
        static constexpr VkExtent3D smaa_area_image_extent = {160, 560, 1};
        static constexpr VkFormat smaa_area_image_format = VK_FORMAT_R8G8_UNORM;
        VkImage device_smaa_area_image = VK_NULL_HANDLE;
        VkImageView device_smaa_area_image_view = VK_NULL_HANDLE;

        static constexpr VkExtent3D smaa_search_image_extent = {64, 16, 1 };
        static constexpr VkFormat smaa_search_image_format = VK_FORMAT_R8_UNORM;
        VkImage device_smaa_search_image = VK_NULL_HANDLE;
        VkImageView device_smaa_search_image_view = VK_NULL_HANDLE;

        // Runtime sized images for the smaa algorithm
        VkImage device_smaa_stencil_image = VK_NULL_HANDLE;
        VkImageView device_smaa_stencil_image_view = VK_NULL_HANDLE;

        VkImage device_smaa_data_image = VK_NULL_HANDLE;
        VkImageView device_smaa_data_edge_image_view = VK_NULL_HANDLE;
        VkImageView device_smaa_data_weight_image_view = VK_NULL_HANDLE;

        // Descriptor layouts for the pipelines
        std::array<VkDescriptorSet, 3> smaa_descriptor_sets = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        // Framebuffers for every pipeline
        std::array<VkFramebuffer, 3> framebuffers = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        std::array<VkPipelineLayout, 3> smaa_pipelines_layout = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkPipeline, 3> smaa_pipelines = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        void create_edge_pipeline(const pipeline_common_structures &common_structures, std::string shader_dir_path);
        void create_weight_pipeline(const pipeline_common_structures &common_structures, std::string shader_dir_path);
        void create_blend_pipeline(const pipeline_common_structures &common_structures, std::string shader_dir_path);
        void load_resource_images_to_host_memory(std::string resource_images_dir_path, const VkPhysicalDeviceMemoryProperties &memory_properties);
        void create_image_views();
        void create_framebuffers(VkImageView out_image_view);
};

#endif //BASE_VULKAN_APP_SMAA_CONTEXT_H
