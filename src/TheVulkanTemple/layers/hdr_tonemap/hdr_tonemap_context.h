#ifndef BASE_VULKAN_APP_HDR_TONEMAP_CONTEXT_H
#define BASE_VULKAN_APP_HDR_TONEMAP_CONTEXT_H

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include "../../external/volk.h"

class HDRTonemapContext {
    public:
        HDRTonemapContext(VkDevice device, VkFormat input_image_format, VkFormat global_ao_image_format, VkFormat out_format);
        ~HDRTonemapContext();

        void create_resources(VkExtent2D screen_res, std::string shader_dir_path);

        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView input_ao_image_view, std::vector<VkImageView> out_image_views);
        void record_into_command_buffer(VkCommandBuffer command_buffer, uint32_t out_image_index, VkExtent2D out_image_size);
    private:
        VkDevice device;
        VkDescriptorSetLayout hdr_tonemap_set_layout;

        VkDescriptorSet hdr_tonemap_descriptor_set = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> hdr_tonemap_framebuffers;

        VkExtent2D out_image_res;

        VkRenderPass hdr_tonemap_render_pass = VK_NULL_HANDLE;
        VkPipelineLayout hdr_tonemap_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline hdr_tonemap_pipeline = VK_NULL_HANDLE;
};


#endif //BASE_VULKAN_APP_HDR_TONEMAP_CONTEXT_H
