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
        HbaoContext(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties, VkExtent2D screen_res,
                    VkFormat depth_image_format,VkFormat out_ao_image_format, std::string shader_dir_path, bool generate_normals);
        ~HbaoContext();

        std::vector<VkImage> get_device_images();
        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void create_resources(VkExtent2D screen_res);
        void init_resources();

        void update_constants(glm::mat4 proj);

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView depth_image_view, VkImageView normal_g_buffer_image_view, VkImageView out_ao_image_view);
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

        struct pipeline_common_structures {
            VkPipelineShaderStageCreateInfo vertex_shader_stage;
            VkPipelineVertexInputStateCreateInfo vertex_input;
            VkPipelineInputAssemblyStateCreateInfo input_assembly;
            VkPipelineRasterizationStateCreateInfo rasterization;
            VkPipelineMultisampleStateCreateInfo multisample;
            VkPipelineDepthStencilStateCreateInfo depth_stencil;
            VkPipelineColorBlendAttachmentState color_blend_attachment;
        };

        std::mt19937 random_engine;
        std::uniform_real_distribution<float> distribution;

        VkDevice device;
        VkExtent2D screen_extent;
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
        bool generate_normals;
        float blur_sharpness;

        VkSampler do_nothing_sampler = VK_NULL_HANDLE;

        VkImage device_linearized_depth_image = VK_NULL_HANDLE;
        VkImageView device_linearized_depth_image_view = VK_NULL_HANDLE;

        VkImage device_view_space_normal_image = VK_NULL_HANDLE;
        VkImageView device_view_space_normal_image_view = VK_NULL_HANDLE;

        VkImage device_deinterleaved_depth_image = VK_NULL_HANDLE;
        std::array<VkImageView, 16> device_deinterleaved_depth_image_views = {};
        VkImageView device_deinterleaved_depth_16_layers_image_view = VK_NULL_HANDLE;

        VkImage device_hbao_calc_image = VK_NULL_HANDLE;
        VkImageView device_hbao_calc_16_layers_image_view = VK_NULL_HANDLE;

        VkImage device_reinterleaved_hbao_image = VK_NULL_HANDLE;
        std::array<VkImageView, 2> device_reinterleaved_hbao_image_view = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        // Buffer that holds the HbaoData both in host and device memory
        VkBuffer host_hbao_data_buffer = VK_NULL_HANDLE;
        VkDeviceMemory host_hbao_data_memory = VK_NULL_HANDLE;
        HbaoData* hbao_data;

        VkBuffer device_hbao_data_buffer = VK_NULL_HANDLE;
        VkDeviceMemory device_hbao_data_memory = VK_NULL_HANDLE;

        // depth_linearize, deinterleave, hbao_calc, hbao_reinterleave, view_normal(!)
        enum stage_name {
            DEPTH_LINEARIZE = 0,
            VIEW_NORMAL = 1,
            DEPTH_DEINTERLEAVE = 2,
            HBAO_CALC = 3,
            HBAO_REINTERLEAVE = 4,
            HBAO_BLUR = 5,
            HBAO_BLUR_2 = 6
        };

        struct stage_data {
            VkRenderPass render_pass = VK_NULL_HANDLE;
            VkDescriptorSetLayout* descriptor_set_layout = nullptr;
            VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
            VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
            VkPipeline pipeline = VK_NULL_HANDLE;
        };
        std::array<stage_data, 7> stages;

        std::array<VkDescriptorSetLayout, 4> hbao_descriptor_set_layouts = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        VkFramebuffer depth_linearize_framebuffer = VK_NULL_HANDLE;
        VkFramebuffer view_normal_framebuffer = VK_NULL_HANDLE;
        std::array<VkFramebuffer, 2> deinterleave_framebuffers = {VK_NULL_HANDLE, VK_NULL_HANDLE};
        VkFramebuffer hbao_calc_framebuffer = VK_NULL_HANDLE;
        VkFramebuffer reinterleave_framebuffer = VK_NULL_HANDLE;
        std::array<VkFramebuffer, 2> hbao_blur_framebuffers = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        void fill_full_screen_pipeline_common_structures(std::string vertex_shader_path, pipeline_common_structures &structures);

        void create_linearize_depth_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);
        void create_view_normal_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);
        void create_deinterleave_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);
        void create_hbao_calc_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);
        void create_reinterleave_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);
        void create_hbao_blur_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);
        void create_hbao_blur_2_pipeline(const pipeline_common_structures &structures, VkExtent2D render_target_extent, std::string frag_shader_path);

        void create_hbao_data_buffers();

        void create_image_views();
        void create_framebuffers(VkImageView depth_image_view, VkImageView out_ao_image_view);
};


#endif //THEVULKANTEMPLE_HBAO_CONTEXT_H
