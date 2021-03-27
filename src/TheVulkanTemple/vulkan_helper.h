#ifndef VULKAN_HELPER_H
#define VULKAN_HELPER_H

#include <vector>
#include <string>
#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include "volk.h"

namespace vulkan_helper {
    enum class Error {
        LOADED_MODEL_IS_NOT_PBR,
        VOLK_INITIALIZATION_FAILED,
        INSTANCE_CREATION_FAILED,
        DEBUG_REPORT_CALLBACK_CREATION_FAILED,
        GLFW_INITIALIZATION_FAILED,
        GLFW_WINDOW_CREATION_FAILED,
        SURFACE_CREATION_FAILED,
        PHYSICAL_DEVICES_ENUMERATION_FAILED,
        NO_AVAILABLE_DEVICE_FOUND,
        DEVICE_CREATION_FAILED,
        PRESENT_MODES_RETRIEVAL_FAILED,
        SURFACE_CAPABILITIES_RETRIEVAL_FAILED,
        SURFACE_FORMATS_RETRIEVAL_FAILED,
        SWAPCHAIN_CREATION_FAILED,
        SWAPCHAIN_IMAGES_RETRIEVAL_FAILED,
        COMMAND_POOL_CREATION_FAILED,
        COMMAND_BUFFER_CREATION_FAILED,
        MODEL_LOADING_FAILED,
        BUFFER_CREATION_FAILED,
        MEMORY_ALLOCATION_FAILED,
        BIND_BUFFER_MEMORY_FAILED,
        BIND_IMAGE_MEMORY_FAILED,
        POINTER_REQUEST_FOR_HOST_MEMORY_FAILED,
        MAPPED_MEMORY_FLUSH_FAILED,
        IMAGE_CREATION_FAILED,
        IMAGE_VIEW_CREATION_FAILED,
        SAMPLER_CREATION_FAILED,
        QUEUE_SUBMIT_FAILED,
        DESCRIPTOR_SET_LAYOUT_CREATION_FAILED,
        DESCRIPTOR_SET_ALLOCATION_FAILED,
        RENDER_PASS_CREATION_FAILED,
        FRAMEBUFFER_CREATION_FAILED,
        SHADER_MODULE_CREATION_FAILED,
        ACQUIRE_NEXT_IMAGE_FAILED,
        QUEUE_PRESENT_FAILED,
        FFX_CACAO_INIT_FAILED
    };

	VkPresentModeKHR select_presentation_mode(const std::vector<VkPresentModeKHR>& presentation_modes, VkPresentModeKHR desired_presentation_mode);
	uint32_t select_number_of_images(const VkSurfaceCapabilitiesKHR& surface_capabilities);
	VkExtent2D select_size_of_images(const VkSurfaceCapabilitiesKHR& surface_capabilities, VkExtent2D desired_size_of_images);
	VkImageUsageFlags select_image_usage(const VkSurfaceCapabilitiesKHR& surface_capabilities,VkImageUsageFlags desired_usages);
	VkSurfaceTransformFlagBitsKHR select_surface_transform(const VkSurfaceCapabilitiesKHR& surface_capabilities, VkSurfaceTransformFlagBitsKHR desired_transform);
	VkSurfaceFormatKHR select_surface_format(const std::vector<VkSurfaceFormatKHR>& surface_formats, VkSurfaceFormatKHR desired_surface_format);
	uint32_t select_memory_index(const VkPhysicalDeviceMemoryProperties& physical_device_memory_properties, const VkMemoryRequirements& memory_requirements, VkMemoryPropertyFlags memory_properties);
	VkBool32 debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData);

    VkBool32 compare_device_features_struct(const void* base, const void* requested, uint32_t size);

    void* create_device_feature_struct_chain(const std::vector<const char*> &device_extensions);
    VkBool32 compare_device_feature_struct_chain(const void *base, const void* requested);

	void normalize_vectors(glm::vec3* vectors, int number_of_elements);

	void interleave(void* dst, glm::vec3* vertices, glm::vec2* tx_coord, glm::vec3* normals, glm::vec4* tangents, int elements);

    int get_binary_file_content(const std::string &file_path, std::vector<uint8_t> &file_binary_content);
    int get_binary_file_content(const std::string &file_path, uint64_t &size, void *dst_ptr);

	uint32_t get_alignment_memory(uint64_t mem_size, uint32_t alignment);
    uint32_t get_aligned_memory_size(uint64_t mem_size, uint32_t alignment);

    void check_error(int32_t last_return_value, Error error_location);

    void insert_or_sum(std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> &target, const std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> &to_sum);
    std::vector<VkDescriptorPoolSize> convert_map_to_vector(const std::unordered_map<VkDescriptorType, uint32_t> &target);

    uint32_t get_mipmap_count(VkExtent3D image_extent);
}
#endif //VULKAN_HELPER_H