#ifndef VULKAN_HELPER_H
#define VULKAN_HELPER_H

#include <vector>
#include <string>
#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <tiny_gltf.h>
#include "volk.h"

namespace vulkan_helper {
	struct model_data_info {
		uint32_t interleaved_mesh_data_size;
		uint32_t vertices;

		uint32_t index_data_size;
		uint32_t indices;
		VkIndexType index_data_type;

        VkExtent3D image_size;
		uint32_t image_layers;
	};

    struct ObjectRenderInfo {
        uint32_t uniform_data_size;
        VkBuffer data_buffer;
        uint64_t mesh_data_offset;
        uint64_t index_data_offset;
        uint64_t indices;
        VkIndexType index_data_type;
    };

	constexpr int v_model_attributes_max_set_bits = 4;
	enum v_model_attributes {
		V_VERTEX = 1, //00000001
        V_TEX_COORD = 2,//00000010
        V_NORMAL = 4, //00000100
		V_TANGENT = 8, //000001000
		V_ALL = 15 //000001111
	};

    constexpr int t_model_attributes_max_set_bits = 4;
	enum t_model_attributes {
		T_ALBEDO_MAP = 1,
		T_ORM_MAP = 2,
        T_NORMAL_MAP = 4,
		T_EMISSIVE_MAP = 8,
		T_ALL = 15
	};

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
        QUEUE_PRESENT_FAILED
    };

	VkPresentModeKHR select_presentation_mode(const std::vector<VkPresentModeKHR>& presentation_modes, VkPresentModeKHR desired_presentation_mode);
	uint32_t select_number_of_images(const VkSurfaceCapabilitiesKHR& surface_capabilities);
	VkExtent2D select_size_of_images(const VkSurfaceCapabilitiesKHR& surface_capabilities, VkExtent2D desired_size_of_images);
	VkImageUsageFlags select_image_usage(const VkSurfaceCapabilitiesKHR& surface_capabilities,VkImageUsageFlags desired_usages);
	VkSurfaceTransformFlagBitsKHR select_surface_transform(const VkSurfaceCapabilitiesKHR& surface_capabilities, VkSurfaceTransformFlagBitsKHR desired_transform);
	VkSurfaceFormatKHR select_surface_format(const std::vector<VkSurfaceFormatKHR>& surface_formats, VkSurfaceFormatKHR desired_surface_format);
	uint32_t select_memory_index(const VkPhysicalDeviceMemoryProperties& physical_device_memory_properties, const VkMemoryRequirements& memory_requirements, VkMemoryPropertyFlags memory_properties);
	VkBool32 debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData);
	VkBool32 compare_physical_device_features_structs(VkPhysicalDeviceFeatures base, VkPhysicalDeviceFeatures requested);
    VkBool32 compare_physical_device_descriptor_indexing_features_structs(VkPhysicalDeviceDescriptorIndexingFeaturesEXT base,
                                                                          VkPhysicalDeviceDescriptorIndexingFeaturesEXT requested);

	void normalize_vectors(glm::vec3* vectors, int number_of_elements);

	void interleave(void* dst, glm::vec3* vertices, glm::vec2* tx_coord, glm::vec3* normals, glm::vec4* tangents, int elements);

    int get_binary_file_content(const std::string &file_path, std::vector<uint8_t> &file_binary_content);
    int get_binary_file_content(const std::string &file_path, uint64_t &size, void *dst_ptr);

	uint32_t get_alignment_memory(uint64_t mem_size, uint32_t alignment);
    uint32_t get_aligned_memory_size(uint64_t mem_size, uint32_t alignment);

    int copy_gltf_contents(tinygltf::Model &model,
                           uint8_t v_attributes_to_copy,
                           bool vertex_normalize,
                           bool index_resolve,
                           uint8_t t_attributes_to_copy,
                           void *dst_ptr, vulkan_helper::model_data_info &out_data);

	uint64_t get_model_data_total_size(const model_data_info &model);
    uint64_t get_model_mesh_and_index_data_size(const model_data_info &model);
    uint64_t get_model_texture_size(const model_data_info &model);

    void check_error(int32_t last_return_value, Error error_location);

    void insert_or_sum(std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> &target, const std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> &to_sum);
    std::vector<VkDescriptorPoolSize> convert_map_to_vector(const std::unordered_map<VkDescriptorType, uint32_t> &target);

    uint32_t get_mipmap_count(VkExtent3D image_extent);
}
#endif //VULKAN_HELPER_H