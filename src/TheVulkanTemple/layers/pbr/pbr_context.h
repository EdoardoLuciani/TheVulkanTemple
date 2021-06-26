#ifndef THEVULKANTEMPLE_PBR_CONTEXT_H
#define THEVULKANTEMPLE_PBR_CONTEXT_H

#include "../../volk.h"
#include "../../vulkan_helper.h"
#include "../../gltf_model.h"

class PbrContext {
    public:
        PbrContext(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties, VkFormat out_depth_image_format,
                   VkFormat out_color_image_format, VkFormat out_normal_image_format);
        ~PbrContext();

        void create_pipeline(std::string shader_dir_path, VkDescriptorSetLayout pbr_model_data_set_layout,
                             VkDescriptorSetLayout camera_data_set_layout, VkDescriptorSetLayout light_data_set_layout);

        void set_output_images(VkExtent2D screen_res, VkImageView out_depth_image, VkImageView out_color_image, VkImageView out_normal_image);
        void record_into_command_buffer(VkCommandBuffer command_buffer, VkDescriptorSet camera_descriptor_set, VkDescriptorSet light_descriptor_set,
                                        const std::vector<VkDescriptorSet> &object_descriptor_sets, const std::vector<vk_object_render_info> &object_render_info);

    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
        VkRenderPass pbr_render_pass = VK_NULL_HANDLE;

        VkPipelineLayout pbr_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline pbr_pipeline = VK_NULL_HANDLE;

        //default value set for creating the dynamic pipeline and then set in set_output_images
        VkExtent2D screen_res = {500, 500};
        VkFramebuffer pbr_framebuffer = VK_NULL_HANDLE;
};


#endif //THEVULKANTEMPLE_PBR_CONTEXT_H
