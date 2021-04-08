#ifndef THEVULKANTEMPLE_HBAO_CONTEXT_H
#define THEVULKANTEMPLE_HBAO_CONTEXT_H
#include "../volk.h"
#include "../vulkan_helper.h"
#include <string>
#include <array>
#include <utility>
#include <unordered_map>

class HbaoContext {
    public:
        HbaoContext(VkDevice device, VkExtent2D screen_res, VkFormat depth_image_format, std::string shader_dir_path);
        ~HbaoContext();

        std::array<VkImage, 1> get_device_images();
        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void create_resources(VkExtent2D screen_res);
        void init_resources();

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView depth_image_view);
        void record_into_command_buffer(VkCommandBuffer command_buffer, VkExtent2D out_image_size, float znear, float zfar, bool is_perspective);

    private:
        VkDevice device;
        VkExtent2D screen_extent;

        // depth_linearize, hbao_
        VkImage device_linearized_depth_image = VK_NULL_HANDLE;
        VkImageView device_linearized_depth_image_view = VK_NULL_HANDLE;

        std::array<VkRenderPass, 3> hbao_render_passes = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkDescriptorSetLayout, 3> hbao_descriptor_set_layouts = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        std::array<VkPipeline, 3> hbao_pipelines = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkPipelineLayout, 3> hbao_pipelines_layouts = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        std::array<VkFramebuffer, 3> hbao_framebuffers = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<VkDescriptorSet, 3> hbao_descriptor_sets = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

        void create_linearize_depth_pipeline(VkShaderModule vertex_shader_module, VkExtent2D render_target_extent, std::string shader_dir_path);

        void create_image_views();
        void create_framebuffers(VkImageView depth_image_view);


};


#endif //THEVULKANTEMPLE_HBAO_CONTEXT_H
