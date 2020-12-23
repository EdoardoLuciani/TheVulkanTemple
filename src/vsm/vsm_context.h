#ifndef BASE_VULKAN_APP_VSM_CONTEXT_H
#define BASE_VULKAN_APP_VSM_CONTEXT_H
#include "../volk.h"
#include <array>
#include <unordered_map>
#include <utility>
#include <vector>

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

        std::pair<VkBuffer,std::vector<VkImage>> get_device_buffer_and_images();
        std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

        void create_resources(std::vector<VkExtent2D> depth_images_res);
        void init_resources(VkCommandPool command_pool, VkCommandBuffer command_buffer, VkQueue queue);
        void allocate_descriptor_sets(VkDescriptorPool descriptor_pool);
    private:
        VkDevice device = VK_NULL_HANDLE;
        VkSampler device_shadow_map_sampler = VK_NULL_HANDLE;
        VkSampler device_max_aniso_linear_sampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout vsm_descriptor_set_layout = VK_NULL_HANDLE;

        std::vector<VkExtent2D> depth_images_res;

        std::vector<VkImage> device_vsm_depth_images;
        std::vector<std::array<VkImageView, 2>> device_vsm_depth_image_views;
        VkBuffer device_vsm_extent_buffer = VK_NULL_HANDLE;

        // We have the same layout for two different descriptor_sets
        std::vector<VkDescriptorSet> vsm_descriptor_sets;

        // Vulkan methods
        void create_image_views();
};

#endif //BASE_VULKAN_APP_VSM_CONTEXT_H
