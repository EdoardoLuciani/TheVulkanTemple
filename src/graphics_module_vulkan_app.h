#ifndef BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
#define BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H

#include "base_vulkan_app.h"

struct model_info {
    uint32_t vertices;
    uint32_t indices;
    VkIndexType index_data_type;
    uint32_t mesh_data_memory_offset;
};

class GraphicsModuleVulkanApp: public BaseVulkanApp {
    public:
        // Inheriting BaseVulkanApp constructor
        using BaseVulkanApp::BaseVulkanApp;
        ~GraphicsModuleVulkanApp();

		// Directly copy data from disk to VRAM
		void load_3d_objects(std::vector<std::string> model_path, uint32_t object_matrix_size);
    private:
        std::vector<model_info> object_info;
        VkBuffer device_mesh_data_buffer = VK_NULL_HANDLE;
        std::vector<VkImage> device_model_images;
        std::vector<std::vector<VkImageView>> device_model_images_views;
        VkDeviceMemory device_model_data_memory = VK_NULL_HANDLE;

};


#endif //BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
