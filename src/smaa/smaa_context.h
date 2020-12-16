#ifndef BASE_VULKAN_APP_SMAA_CONTEXT_H
#define BASE_VULKAN_APP_SMAA_CONTEXT_H
#include "../volk.h"
#include <array>
#include <string>

class SmaaContext {
    public:
        SmaaContext(VkDevice device, VkExtent2D screen_res);
        ~SmaaContext();
        std::array<VkImage, 4> get_device_images();
        void upload_resource_images_to_device_memory(std::string area_tex_path, std::string search_tex_path, const VkPhysicalDeviceMemoryProperties &memory_properties,
                                                     VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue);
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

        void create_image_views_and_samplers();
};

#endif //BASE_VULKAN_APP_SMAA_CONTEXT_H
