#ifndef BASE_VULKAN_APP_VSM_CONTEXT_H
#define BASE_VULKAN_APP_VSM_CONTEXT_H
#include "../volk.h"
#include <array>

class VSMContext {
    public:
        VSMContext(VkDevice device, VkExtent2D screen_res);
        ~VSMContext();
        VkImage get_device_image();
        void create_image_views();
    private:
        VkDevice device = VK_NULL_HANDLE;

        VkImage device_vsm_depth_image = VK_NULL_HANDLE;
        std::array<VkImageView, 2> device_vsm_depth_image_views = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        VkSampler device_shadow_map_sampler = VK_NULL_HANDLE;
};

#endif //BASE_VULKAN_APP_VSM_CONTEXT_H
