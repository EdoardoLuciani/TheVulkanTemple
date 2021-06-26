#ifndef BASE_VULKAN_APP_VSM_CONTEXT_H
#define BASE_VULKAN_APP_VSM_CONTEXT_H
#include "../../volk.h"
#include <array>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string>
#include "../../vulkan_helper.h"
#include "../../gltf_model.h"

/* To use this class, it is required to call the correct sequence of calls, first the
 * constructor VSMContext, then you need to allocate a VkDevice memory big enough for
 * the image that you get from get_device_image(). Allocation is left to the user for
 * performance reasons. Then you can safely call create_image_views(). For the descriptor
 * sets you first need to query how many elements you need to allocate, then pass the
 * pool to allocate_descriptor_sets.
 */

class VSMContext {
public:
    VSMContext(VkDevice device);
    ~VSMContext();

    std::vector<VkImage> get_device_images();
    std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();
    VkImageView get_image_view(int index);

    void create_resources(std::vector<VkExtent2D> depth_images_res, std::string shader_dir_path,
                          VkDescriptorSetLayout pbr_model_set_layout, VkDescriptorSetLayout light_set_layout);
    void init_resources();
    void allocate_descriptor_sets(VkDescriptorPool descriptor_pool);
    void record_into_command_buffer(VkCommandBuffer command_buffer, std::vector<VkDescriptorSet> object_data_set,
                                    VkDescriptorSet light_data_set, const std::vector<vk_object_render_info> &object_render_info);
private:
    std::vector<VkExtent2D> depth_images_res;

    VkDevice device = VK_NULL_HANDLE;
    VkSampler device_render_target_sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout vsm_descriptor_set_layout = VK_NULL_HANDLE;
    VkRenderPass shadow_map_render_pass = VK_NULL_HANDLE;

    std::vector<VkImage> device_vsm_depth_images;
    std::vector<std::array<VkImageView, 2>> device_vsm_depth_image_views;

    std::vector<VkImage> device_light_depth_images;
    std::vector<VkImageView> device_light_depth_image_views;

    std::vector<VkFramebuffer> framebuffers;

    // We have the same layout for two different descriptor_sets
    std::vector<VkDescriptorSet> vsm_descriptor_sets;

    // Shadow map render pipeline
    VkPipelineLayout shadow_map_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline shadow_map_pipeline = VK_NULL_HANDLE;

    // Gaussian blur pipeline
    VkPipelineLayout gaussian_blur_pipeline_layout = VK_NULL_HANDLE;
    std::array<VkPipeline,2> gaussian_blur_xy_pipelines = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    // Vulkan methods
    void create_image_views();
    void create_framebuffers();
    void create_shadow_map_pipeline(std::string shader_dir_path, VkDescriptorSetLayout pbr_model_set_layout, VkDescriptorSetLayout light_set_layout);
    void create_gaussian_blur_pipelines(std::string shader_dir_path);
};
#endif //BASE_VULKAN_APP_VSM_CONTEXT_H
