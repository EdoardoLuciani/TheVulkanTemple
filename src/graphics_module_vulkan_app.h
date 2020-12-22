#ifndef BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
#define BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H

#include <array>
#include "base_vulkan_app.h"
#include "smaa/smaa_context.h"
#include "vsm/vsm_context.h"

struct ModelInfo {
    uint32_t vertices;
    uint32_t indices;
    VkIndexType index_data_type;
    uint32_t mesh_data_memory_offset;
};

struct EngineOptions {
    int anti_aliasing;
    int shadows;
    int HDR;
};

class GraphicsModuleVulkanApp: public BaseVulkanApp {
    public:
        // Inheriting BaseVulkanApp constructor
        GraphicsModuleVulkanApp(const std::string &application_name,
                      std::vector<const char*> &desired_instance_level_extensions,
                      VkExtent2D window_size,
                      const std::vector<const char*> &desired_device_level_extensions,
                      const VkPhysicalDeviceFeatures &required_physical_device_features,
                      VkBool32 surface_support,
                      EngineOptions options);
        ~GraphicsModuleVulkanApp();

		// Directly copy data from disk to VRAM
		void load_3d_objects(std::vector<std::string> model_path, uint32_t object_matrix_size);
    private:
        EngineOptions engine_options;

        // Image for depth comparison
        VkImage device_depth_image = VK_NULL_HANDLE;
        VkImageView device_depth_image_view = VK_NULL_HANDLE;

        // HDR ping pong image
        VkImage device_render_target = VK_NULL_HANDLE;
        std::array<VkImageView, 2> device_render_target_image_views = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        SmaaContext smaa_context;
        VSMContext vsm_context;

        // Memory in which all attachment reside
        VkDeviceMemory device_attachments_memory = VK_NULL_HANDLE;

        // Descriptor things
        VkDescriptorSetLayout pbr_model_data_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout light_data_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout camera_data_set_layout = VK_NULL_HANDLE;

        VkDescriptorPool attachments_descriptor_pool = VK_NULL_HANDLE;

        // Uniform data
        VkBuffer host_model_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_model_uniform_memory = VK_NULL_HANDLE;
        void *host_model_uniform_buffer_ptr;

        // Models data
        VkBuffer device_mesh_data_buffer = VK_NULL_HANDLE;
        std::vector<VkImage> device_model_images;
        std::vector<std::vector<VkImageView>> device_model_images_views;
        VkDeviceMemory device_model_data_memory = VK_NULL_HANDLE;
        std::vector<ModelInfo> object_info;

        // Vulkan methods
        void create_descriptor_sets();

        // Helper methods
        void create_buffer(VkBuffer &buffer, uint64_t size, VkBufferUsageFlags usage);
        void create_image(VkImage &image, VkFormat format, VkExtent3D image_size, uint32_t layers, VkImageUsageFlags usage_flags, VkImageCreateFlags create_flags = 0);
        void create_image_view(VkImageView &image_view, VkImage image, VkFormat image_format, VkImageAspectFlags aspect_mask, uint32_t start_layer, uint32_t layer_count);
        void allocate_and_bind_to_memory(VkDeviceMemory &memory, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VkMemoryPropertyFlags flags);
        void submit_command_buffers(std::vector<VkCommandBuffer> command_buffers, VkPipelineStageFlags stage_flags,
                                    std::vector<VkSemaphore> wait_semaphores, std::vector<VkSemaphore> signal_semaphores, VkFence fence = VK_NULL_HANDLE);
};

#endif //BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
