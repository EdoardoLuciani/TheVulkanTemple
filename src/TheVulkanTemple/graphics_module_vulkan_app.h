#ifndef BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
#define BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H

#include <vector>
#include <array>
#include <functional>
#include <optional>
#include "base_vulkan_app.h"
#include "layers/smaa/smaa_context.h"
#include "layers/vsm/vsm_context.h"
#include "layers/hbao/hbao_context.h"
#include "layers/pbr/pbr_context.h"
#include "layers/hdr_tonemap/hdr_tonemap_context.h"
#include "layers/amd_fsr/amd_fsr.h"
#include "vulkan_helper.h"
#include "external/vk_mem_alloc.h"
#include <glm/glm.hpp>
#include "camera.h"
#include "light.h"
#include "gltf_model.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

struct EngineOptions {
    AmdFsr::Settings fsr_settings;
};

class GraphicsModuleVulkanApp : public BaseVulkanApp {
    public:
        // Inheriting BaseVulkanApp constructor
        GraphicsModuleVulkanApp(const std::string &application_name,
                      VkExtent2D window_size,
                      bool fullscreen,
                      EngineOptions options);

        ~GraphicsModuleVulkanApp();

		// Directly copy data from disk to VRAM
		void load_3d_objects(std::vector<std::pair<std::string, glm::mat4>> model_file_matrix);
        void load_lights(std::vector<Light> &&lights);
        void set_camera(Camera &&camera);
        void init_renderer();

        void start_frame_loop(std::function<void(GraphicsModuleVulkanApp*)> resize_callback,
                              std::function<void(GraphicsModuleVulkanApp*, uint32_t)> frame_start);

        // Methods to manage the scene objects
        Camera* get_camera_ptr() { return &camera; };
        const Light* get_light_ptr(uint32_t idx) { return &lights_container.at(idx); };
        VkModel* get_gltf_model_ptr(uint32_t idx) { return &vk_models.at(idx); };
    private:
        EngineOptions engine_options;
		VkExtent2D rendering_resolution;
        VmaAllocator vma_allocator;
        VkSampler max_aniso_linear_sampler;

        std::vector<VkModel> vk_models;
        // Model uniform data
        VkBuffer host_model_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation host_model_uniform_allocation = VK_NULL_HANDLE;
        void *host_model_uniform_buffer_ptr;
        VkBuffer device_model_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation device_model_uniform_allocation = VK_NULL_HANDLE;
        // Models data
        VkBuffer device_mesh_data_buffer = VK_NULL_HANDLE;
        VmaAllocation device_mesh_data_allocation = VK_NULL_HANDLE;
        //std::vector<VkImage> device_model_images;
        //std::vector<VmaAllocation> device_model_images_allocations;
        //std::vector<VkImageView> device_model_images_views;
        //std::vector<VkSampler> model_image_samplers;

        // Conteiner that makes possible to iterate through shadowed and non-shadowed lights separately while also indexing them randomly
        typedef boost::multi_index_container<
            Light,
            boost::multi_index::indexed_by<
                boost::multi_index::random_access<>,
                boost::multi_index::ordered_non_unique<
                    BOOST_MULTI_INDEX_MEMBER(Light, uint32_t, shadow_map_height)
                >
            >
        > LightsContainer;
        LightsContainer lights_container;

        const uint32_t MAX_SHADOWED_LIGHTS = 8;
        Camera camera;
        // Host buffer and allocation for the camera and lights data
        VkBuffer host_camera_lights_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation host_camera_lights_allocation = VK_NULL_HANDLE;
        void *host_camera_lights_data;
        VkBuffer device_camera_lights_uniform_buffer = VK_NULL_HANDLE;
        VmaAllocation device_camera_lights_uniform_allocation = VK_NULL_HANDLE;

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
        // Image for HDRTonemap output
        VkImage device_tonemapped_image = VK_NULL_HANDLE;
		VkImageView device_tonemapped_image_view = VK_NULL_HANDLE;
		// Image for AmdFSR
		VkImage device_upscaled_image = VK_NULL_HANDLE;
		VkImageView device_upscaled_image_view = VK_NULL_HANDLE;

        // Contexts for graphical effects
        VSMContext vsm_context;
        PbrContext pbr_context;
        SmaaContext smaa_context;
        HbaoContext hbao_context;
        HDRTonemapContext hdr_tonemap_context;
        std::unique_ptr<AmdFsr> amd_fsr = nullptr;

        // Static allocations for the layers
        VmaAllocation amd_fsr_uniform_allocation = VK_NULL_HANDLE;

        // Allocations in which all attachment reside
        std::vector<VmaAllocation> device_attachments_allocations;

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
        void allocate_and_bind_to_memory(std::vector<VmaAllocation> &out_allocations, const std::vector<VkBuffer> &buffers, const std::vector<VkImage> &images, VmaMemoryUsage vma_memory_usage);
        void allocate_and_bind_to_memory_buffer(VmaAllocation &out_allocation, VkBuffer buffer, VmaMemoryUsage vma_memory_usage);
        void allocate_and_bind_to_memory_image(VmaAllocation &out_allocation, VkImage image, VmaMemoryUsage vma_memory_usage);
        void submit_command_buffers(std::vector<VkCommandBuffer> command_buffers, VkPipelineStageFlags stage_flags,
                                    std::vector<VkSemaphore> wait_semaphores, std::vector<VkSemaphore> signal_semaphores, VkFence fence = VK_NULL_HANDLE);

        // Static methods used for filling the BaseVulkanApp structure
        static std::vector<const char*> get_instance_extensions();
        static std::vector<const char*> get_device_extensions();
        static VkPhysicalDeviceFeatures2* get_required_physical_device_features(bool delete_static_structure, EngineOptions engine_options);
};

#endif //BASE_VULKAN_APP_GRAPHICS_MODULE_VULKAN_APP_H
