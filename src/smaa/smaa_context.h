#ifndef BASE_VULKAN_APP_SMAA_CONTEXT_H
#define BASE_VULKAN_APP_SMAA_CONTEXT_H
#include "../volk.h"
#include <array>
#include <string>
#include <unordered_map>

class SmaaContext {
    public:
        SmaaContext(VkDevice device, VkExtent2D screen_res);
        ~SmaaContext();

        std::pair<VkBuffer, std::array<VkImage, 4>> get_device_buffers_and_images();

        void init_resources(std::string area_tex_path, std::string search_tex_path, const VkPhysicalDeviceMemoryProperties &memory_properties,
                            VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue);


        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();
        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView depth_image_view);

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkExtent3D screen_extent;

        static constexpr VkExtent3D smaa_area_image_extent = {160, 560, 1};
        static constexpr VkFormat smaa_area_image_format = VK_FORMAT_R8G8_UNORM;
        VkImage device_smaa_area_image = VK_NULL_HANDLE;
        VkImageView device_smaa_area_image_view = VK_NULL_HANDLE;

        static constexpr VkExtent3D smaa_search_image_extent = {64, 16, 1 };
        static constexpr VkFormat smaa_search_image_format = VK_FORMAT_R8_UNORM;
        VkImage device_smaa_search_image = VK_NULL_HANDLE;
        VkImageView device_smaa_search_image_view = VK_NULL_HANDLE;

        VkImage device_smaa_stencil_image = VK_NULL_HANDLE;
        VkImageView device_smaa_stencil_image_view = VK_NULL_HANDLE;

        VkImage device_smaa_data_image = VK_NULL_HANDLE;
        VkImageView device_smaa_data_edge_image_view = VK_NULL_HANDLE;
        VkImageView device_smaa_data_weight_image_view = VK_NULL_HANDLE;

        VkBuffer device_smaa_rt_metrics_buffer = VK_NULL_HANDLE;

        std::array<VkDescriptorSetLayout, 4> smaa_descriptor_sets_layout = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,};
        std::array<VkDescriptorSet, 4> smaa_descriptor_sets = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,};

        VkSampler device_render_target_sampler = VK_NULL_HANDLE;

        void create_image_views();
};

#endif //BASE_VULKAN_APP_SMAA_CONTEXT_H
