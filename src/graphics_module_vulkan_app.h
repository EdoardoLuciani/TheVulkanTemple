#ifndef BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
#define BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H

#include <vector>
#include <array>
#include "base_vulkan_app.h"
#include "smaa/smaa_context.h"
#include "vsm/vsm_context.h"
#include "hdr_tonemap/hdr_tonemap_context.h"
#include "vulkan_helper.h"
#include <glm/glm.hpp>
#include "camera.h"
#include "light.h"

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
                      EngineOptions options,
                      void *addional_structure);
        ~GraphicsModuleVulkanApp();

		// Directly copy data from disk to VRAM
		void load_3d_objects(std::vector<std::string> model_path, uint32_t per_object_uniform_data_size);
        void load_lights(const std::vector<Light> &lights);
        void set_camera(Camera camera);
        void init_renderer();

        uint8_t* get_model_uniform_data_ptr(int model_index);
        void start_frame_loop();
    private:
        EngineOptions engine_options;
        VkExtent3D screen_extent;
        VkSampler device_max_aniso_linear_sampler;

        // Model uniform data
        VkBuffer host_model_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_model_uniform_memory = VK_NULL_HANDLE;
        void *host_model_uniform_buffer_ptr;
        VkBuffer device_model_uniform_buffer = VK_NULL_HANDLE;

        // Models data
        VkBuffer device_mesh_data_buffer = VK_NULL_HANDLE;
        std::vector<VkImage> device_model_images;
        std::vector<std::vector<VkImageView>> device_model_images_views;
        VkDeviceMemory device_model_data_memory = VK_NULL_HANDLE;
        std::vector<vulkan_helper::ObjectRenderInfo> objects_info;

        std::vector<Light> lights;
        Camera camera;

        // Host buffer and memory for the camera and lights data
        VkBuffer host_camera_lights_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_camera_lights_memory = VK_NULL_HANDLE;
        void *host_camera_lights_data;

        // Image for depth comparison
        VkImage device_depth_image = VK_NULL_HANDLE;
        VkImageView device_depth_image_view = VK_NULL_HANDLE;
        // HDR ping pong image
        VkImage device_render_target = VK_NULL_HANDLE;
        std::array<VkImageView, 2> device_render_target_image_views = {VK_NULL_HANDLE, VK_NULL_HANDLE};
        // Device buffer that hold camera and lights information
        VkBuffer device_camera_lights_uniform_buffer = VK_NULL_HANDLE;
        // Todo: add SmaaContext
        //SmaaContext smaa_context;
        VSMContext vsm_context;
        HDRTonemapContext hdr_tonemap_context;
        // Memory in which all attachment reside
        VkDeviceMemory device_attachments_memory = VK_NULL_HANDLE;

        // Descriptor things
        VkDescriptorSetLayout pbr_model_data_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout light_data_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout camera_data_set_layout = VK_NULL_HANDLE;

        std::vector<VkDescriptorSet> descriptor_sets;
        VkDescriptorPool attachments_descriptor_pool = VK_NULL_HANDLE;

        // PBR renderpass
        VkRenderPass pbr_render_pass = VK_NULL_HANDLE;
        // PBR framebuffer
        VkFramebuffer pbr_framebuffer = VK_NULL_HANDLE;
        // PBR pipeline
        VkPipelineLayout pbr_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline pbr_pipeline = VK_NULL_HANDLE;

        std::vector<VkSemaphore> semaphores;

        // Vulkan methods
        void create_render_pass();
        void create_sets_layouts();
        void write_descriptor_sets();
        void create_framebuffers();
        void create_pbr_pipeline();
        void record_command_buffers();

        // Helper methods
        void create_buffer(VkBuffer &buffer, uint64_t size, VkBufferUsageFlags usage);
        void create_image(VkImage &image, VkFormat format, VkExtent3D image_size, uint32_t layers, VkImageUsageFlags usage_flags, VkImageCreateFlags create_flags = 0);
        void create_image_view(VkImageView &image_view, VkImage image, VkFormat image_format, VkImageAspectFlags aspect_mask, uint32_t start_layer, uint32_t layer_count);
        void allocate_and_bind_to_memory(VkDeviceMemory &memory, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VkMemoryPropertyFlags flags);
        void submit_command_buffers(std::vector<VkCommandBuffer> command_buffers, VkPipelineStageFlags stage_flags,
                                    std::vector<VkSemaphore> wait_semaphores, std::vector<VkSemaphore> signal_semaphores, VkFence fence = VK_NULL_HANDLE);
};

#endif //BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
