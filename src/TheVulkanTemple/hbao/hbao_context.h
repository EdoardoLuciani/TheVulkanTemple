#ifndef THEVULKANTEMPLE_HBAO_CONTEXT_H
#define THEVULKANTEMPLE_HBAO_CONTEXT_H
#include "../volk.h"
#include "../vulkan_helper.h"
#include <string>
#include <array>
#include <utility>
#include <unordered_map>
#include <random>

class HbaoContext {
    public:
        HbaoContext(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties, VkExtent2D screen_res, VkFormat depth_image_format, std::string shader_dir_path);
        ~HbaoContext();

        std::array<VkImage, 4> get_device_images();
        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void create_resources(VkExtent2D screen_res);
        void init_resources();

        void update_constants(glm::mat4 proj);

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView depth_image_view, VkImageView normal_g_buffer_image_view);
        void record_into_command_buffer(VkCommandBuffer command_buffer, VkExtent2D out_image_size, float znear, float zfar, bool is_perspective);

    private:
        struct HbaoData {
            float   RadiusToScreen; // radius
            float   R2;             // 1/radius
            float   NegInvR2;       // radius * radius
            float   NDotVBias;

            glm::vec2    InvFullResolution;
            glm::vec2    InvQuarterResolution;

            float   AOMultiplier;
            float   PowExponent;
            glm::vec2    _pad0;

            glm::vec4    projInfo;

            glm::vec2    projScale;
            int     projOrtho;
            int     _pad1;

            glm::vec4    float2Offsets[16];
            glm::vec4    jitters[16];
        };

        std::mt19937 random_engine;
        std::uniform_real_distribution<float> distribution;

        VkDevice device;
        VkExtent2D screen_extent;
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties;

        VkSampler do_nothing_sampler = VK_NULL_HANDLE;

        VkImage device_linearized_depth_image = VK_NULL_HANDLE;
        VkImageView device_linearized_depth_image_view = VK_NULL_HANDLE;

        VkImage device_deinterleaved_depth_image = VK_NULL_HANDLE;
        std::array<VkImageView, 16> device_deinterleaved_depth_image_views = {};
        VkImageView device_deinterleaved_depth_16_layers_image_view = VK_NULL_HANDLE;

        VkImage device_hbao_calc_image = VK_NULL_HANDLE;
        VkImageView device_hbao_calc_16_layers_image_view = VK_NULL_HANDLE;

        VkImage device_reinterleaved_hbao_image = VK_NULL_HANDLE;
        VkImageView device_reinterleaved_hbao_image_view = VK_NULL_HANDLE;

        // Buffer that holds the HbaoData both in host and device memory
        VkBuffer host_hbao_data_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_hbao_data_memory = VK_NULL_HANDLE;
        HbaoData* hbao_data;

        VkBuffer device_hbao_data_buffer = VK_NULL_HANDLE;
        VkDeviceMemory device_hbao_data_memory = VK_NULL_HANDLE;

        // depth_linearize, deinterleave, hbao_calc, hbao_reinterleave
        std::array<VkRenderPass, 4> hbao_render_passes = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkDescriptorSetLayout, 4> hbao_descriptor_set_layouts = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        std::array<VkPipeline, 4> hbao_pipelines = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkPipelineLayout, 4> hbao_pipelines_layouts = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        VkFramebuffer depth_linearize_framebuffer = VK_NULL_HANDLE;
        std::array<VkFramebuffer, 2> deinterleave_framebuffers = {};
        VkFramebuffer hbao_calc_framebuffer = VK_NULL_HANDLE;
        VkFramebuffer reinterleave_framebuffer = VK_NULL_HANDLE;

        std::array<VkDescriptorSet, 4> hbao_descriptor_sets = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        void create_linearize_depth_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path);
        void create_deinterleave_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path);
        void create_hbao_calc_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path);
        void create_reinterleave_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path);

        void create_hbao_data_buffers();

        void create_image_views();
        void create_framebuffers(VkImageView depth_image_view);
};


#endif //THEVULKANTEMPLE_HBAO_CONTEXT_H
