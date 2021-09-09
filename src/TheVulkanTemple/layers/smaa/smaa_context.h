#ifndef BASE_VULKAN_APP_SMAA_CONTEXT_H
#define BASE_VULKAN_APP_SMAA_CONTEXT_H
#include "../../external/volk.h"
#include <array>
#include <string>
#include <unordered_map>

class SmaaContext {
    public:
        SmaaContext(VkDevice device, VkFormat out_image_format);
        ~SmaaContext();

        std::array<VkImage, 4> get_device_images();
        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void create_resources(VkExtent2D screen_res, std::string shader_dir_path);
        void init_resources(std::string support_images_dir, const VkPhysicalDeviceMemoryProperties &memory_properties,
                            VkImageView out_image_view, VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue);

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view);

        void record_into_command_buffer(VkCommandBuffer command_buffer);

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkSampler device_render_target_sampler = VK_NULL_HANDLE;
        std::array<VkDescriptorSetLayout, 3> smaa_descriptor_sets_layout = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkRenderPass, 3> render_passes = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        VkExtent3D screen_extent;

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

        void create_edge_pipeline(std::string shader_dir_path);
        void create_weight_pipeline(std::string shader_dir_path);
        void create_blend_pipeline(std::string shader_dir_path);
        void create_image_views();
        void create_framebuffers(VkImageView out_image_view);
};

#endif //BASE_VULKAN_APP_SMAA_CONTEXT_H
