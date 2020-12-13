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

        VkBuffer host_model_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_model_uniform_memory = VK_NULL_HANDLE;
        void *host_model_uniform_buffer_ptr;

        VkBuffer device_mesh_data_buffer = VK_NULL_HANDLE;
        std::vector<VkImage> device_model_images;
        std::vector<std::vector<VkImageView>> device_model_images_views;
        VkDeviceMemory device_model_data_memory = VK_NULL_HANDLE;

        void create_buffer(VkBuffer &buffer, uint64_t size, VkBufferUsageFlags usage);
        void create_image(VkImage &image,VkExtent3D image_size, uint32_t layers, VkImageUsageFlags usage_flags, VkImageCreateFlags create_flags = 0);
        void create_image_view(VkImageView &image_view, VkImage image, VkFormat image_format, VkImageAspectFlags aspect_mask, uint32_t start_layer, uint32_t layer_count);
        void allocate_and_bind_to_memory(VkDeviceMemory &memory, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VkMemoryPropertyFlags flags);

};


#endif //BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
