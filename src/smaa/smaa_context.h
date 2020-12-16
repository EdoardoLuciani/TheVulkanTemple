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

        static constexpr VkExtent3D area_image_size = {160,560,1};
        VkImage device_smaa_area_image = VK_NULL_HANDLE;
        VkImageView device_smaa_area_image_view = VK_NULL_HANDLE;

        static constexpr VkExtent3D search_image_size = {160,560,1};
        VkImage device_smaa_search_image = VK_NULL_HANDLE;
        VkImageView device_smaa_search_image_view = VK_NULL_HANDLE;

        VkImage device_smaa_stencil_image = VK_NULL_HANDLE;
        VkImageView device_smaa_stencil_image_view = VK_NULL_HANDLE;

        VkImage device_smaa_data_image = VK_NULL_HANDLE;
        VkImageView device_smaa_data_blend_image_view = VK_NULL_HANDLE;
        VkImageView device_smaa_data_edge_image_view = VK_NULL_HANDLE;
};

#endif //BASE_VULKAN_APP_SMAA_CONTEXT_H
