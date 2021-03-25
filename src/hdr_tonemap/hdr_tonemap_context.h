#ifndef BASE_VULKAN_APP_HDR_TONEMAP_CONTEXT_H
#define BASE_VULKAN_APP_HDR_TONEMAP_CONTEXT_H

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include "../volk.h"

class HDRTonemapContext {
    public:
        HDRTonemapContext(VkDevice device);
        ~HDRTonemapContext();

        void create_resources(std::string shader_dir_path, uint32_t out_images_count);

        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView input_ao_image_view, std::vector<VkImage> out_images, VkFormat image_format);
        void record_into_command_buffer(VkCommandBuffer command_buffer, uint32_t out_image_index, VkExtent2D out_image_size);
    private:
        VkDevice device;
        VkSampler device_render_target_sampler;
        VkDescriptorSetLayout hdr_tonemap_set_layout;

        std::vector<VkImage> out_images;
        std::vector<VkImageView> out_images_views;

        std::vector<VkDescriptorSet> hdr_tonemap_descriptor_sets;

        uint32_t out_images_count;

        VkPipelineLayout hdr_tonemap_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline hdr_tonemap_pipeline = VK_NULL_HANDLE;
};


#endif //BASE_VULKAN_APP_HDR_TONEMAP_CONTEXT_H
