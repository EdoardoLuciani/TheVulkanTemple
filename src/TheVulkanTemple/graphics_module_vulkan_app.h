#ifndef BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
#define BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H

#include <vector>
#include <array>
#include <functional>
#include "base_vulkan_app.h"
#include "layers/smaa/smaa_context.h"
#include "layers/vsm/vsm_context.h"
#include "layers/hbao/hbao_context.h"
#include "layers/pbr/pbr_context.h"
#include "layers/hdr_tonemap/hdr_tonemap_context.h"
#include "vulkan_helper.h"
#include <glm/glm.hpp>
#include "camera.h"
#include "light.h"
#include "gltf_model.h"

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
                      bool fullscreen,
                      const std::vector<const char*> &desired_device_level_extensions,
                      const VkPhysicalDeviceFeatures2 &required_physical_device_features,
                      VkBool32 surface_support,
                      EngineOptions options);
        ~GraphicsModuleVulkanApp();

		// Directly copy data from disk to VRAM
		void load_3d_objects(std::vector<GltfModel> gltf_models);
        void load_lights(const std::vector<Light> &lights);
        void set_camera(Camera camera);
        void init_renderer();

        void start_frame_loop(std::function<void(GraphicsModuleVulkanApp*)> resize_callback,
                              std::function<void(GraphicsModuleVulkanApp*, uint32_t)> frame_start);

        // Methods to manage the scene objects
        Camera* get_camera_ptr() { return &camera; };
        Light* get_light_ptr(uint32_t idx) { return &lights.at(idx); };
        GltfModel* get_gltf_model_ptr(uint32_t idx) { return &objects.at(idx).model; };
    private:
        EngineOptions engine_options;
        VkSampler max_aniso_linear_sampler;

        std::vector<vk_object_render_info> objects;
        // Model uniform data
        VkBuffer host_model_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_model_uniform_memory = VK_NULL_HANDLE;
        void *host_model_uniform_buffer_ptr;
        VkBuffer device_model_uniform_buffer = VK_NULL_HANDLE;
        // Models data
        VkBuffer device_mesh_data_buffer = VK_NULL_HANDLE;
        std::vector<VkImage> device_model_images;
        std::vector<VkImageView> device_model_images_views;
        std::vector<VkSampler> model_image_samplers;
        VkDeviceMemory device_model_data_memory = VK_NULL_HANDLE;

        std::vector<Light> lights;
        const uint32_t MAX_SHADOWED_LIGHTS = 8;
        Camera camera;
        // Host buffer and memory for the camera and lights data
        VkBuffer host_camera_lights_uniform_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_camera_lights_memory = VK_NULL_HANDLE;
        void *host_camera_lights_data;
        VkBuffer device_camera_lights_uniform_buffer = VK_NULL_HANDLE;

        // Image for depth comparison
        VkImage device_depth_image = VK_NULL_HANDLE;
        VkImageView device_depth_image_view = VK_NULL_HANDLE;
        // HDR ping pong image
        VkImage device_render_target = VK_NULL_HANDLE;
        std::array<VkImageView, 2> device_render_target_image_views = {VK_NULL_HANDLE, VK_NULL_HANDLE};
        // Image for normal g-buffer
        VkImage device_normal_g_image = VK_NULL_HANDLE;
        VkImageView device_normal_g_image_view = VK_NULL_HANDLE;
        // Image for hbao output
        VkImage device_global_ao_image = VK_NULL_HANDLE;
        VkImageView device_global_ao_image_view = VK_NULL_HANDLE;

        // Contexts for graphical effects
        VSMContext vsm_context;
        SmaaContext smaa_context;
        PbrContext pbr_context;
        HbaoContext hbao_context;
        HDRTonemapContext hdr_tonemap_context;
        // Memory in which all attachment reside
        VkDeviceMemory device_attachments_memory = VK_NULL_HANDLE;

        // Descriptor things
        VkDescriptorSetLayout pbr_model_data_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout light_data_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout camera_data_set_layout = VK_NULL_HANDLE;

        std::vector<VkDescriptorSet> descriptor_sets;
        VkDescriptorPool attachments_descriptor_pool = VK_NULL_HANDLE;

        std::vector<VkSemaphore> semaphores;

        // Vulkan methods
        void create_sets_layouts();
        void write_descriptor_sets();
        void record_command_buffers();
        void on_window_resize(std::function<void(GraphicsModuleVulkanApp*)> resize_callback);

        // Helper methods
        void create_buffer(VkBuffer &buffer, uint64_t size, VkBufferUsageFlags usage);
        void create_image(VkImage &image, VkFormat format, VkExtent3D image_size, uint32_t layers, uint32_t mipmaps_count, VkImageUsageFlags usage_flags, VkImageCreateFlags create_flags = 0);
        void create_image_view(VkImageView &image_view, VkImage image, VkFormat image_format, VkImageAspectFlags aspect_mask, uint32_t start_layer, uint32_t layer_count);
        void allocate_and_bind_to_memory(VkDeviceMemory &memory, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VkMemoryPropertyFlags flags);
        void submit_command_buffers(std::vector<VkCommandBuffer> command_buffers, VkPipelineStageFlags stage_flags,
                                    std::vector<VkSemaphore> wait_semaphores, std::vector<VkSemaphore> signal_semaphores, VkFence fence = VK_NULL_HANDLE);
};

#endif //BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
