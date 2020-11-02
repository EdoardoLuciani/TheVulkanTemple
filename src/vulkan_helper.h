#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// Only for definitions
#include <tiny_gltf.h>

namespace vulkan_helper {
	typedef struct model_data_info {
		uint32_t host_interleaved_vertex_data_offset;
		uint32_t device_interleaved_vertex_data_offset;
		uint32_t interleaved_vertex_data_size;
		uint32_t vertices;

		uint32_t host_index_data_offset;
		uint32_t device_index_data_offset;
		uint32_t index_data_size;
		uint32_t indices;
		VkIndexType single_index_size;

		uint32_t host_image_data_offset;
		uint32_t device_image_data_offset;
		uint32_t image_layers;
		VkExtent3D image_size;
	} model_data_info;

	enum v_model_attributes {
		V_VERTEX,
		V_NORMAL,
		V_TEX_COORD,
		V_TANGENT
	};

	enum t_model_attributes {
		T_ALBEDO_MAP,
		T_NORMAL_MAP,
		T_ORM_MAP,
		T_EMISSIVE_MAP
	};

	VkPresentModeKHR select_presentation_mode(const std::vector<VkPresentModeKHR>& presentation_modes, VkPresentModeKHR desired_presentation_mode);
	uint32_t select_number_of_images(const VkSurfaceCapabilitiesKHR& surface_capabilities);
	VkExtent2D select_size_of_images(const VkSurfaceCapabilitiesKHR& surface_capabilities, VkExtent2D desired_size_of_images);
	VkImageUsageFlags select_image_usage(const VkSurfaceCapabilitiesKHR& surface_capabilities,VkImageUsageFlags desired_usages);
	VkSurfaceTransformFlagBitsKHR select_surface_transform(const VkSurfaceCapabilitiesKHR& surface_capabilities, VkSurfaceTransformFlagBitsKHR desired_transform);
	VkSurfaceFormatKHR select_surface_format(const std::vector<VkSurfaceFormatKHR>& surface_formats, VkSurfaceFormatKHR desired_surface_format);
	uint32_t select_memory_index(const VkPhysicalDeviceMemoryProperties& physical_device_memory_properties, const VkMemoryRequirements& memory_requirements, VkMemoryPropertyFlagBits memory_properties);
	VkBool32 debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData);

	void normalize_vectors(glm::vec3* vectors, int number_of_elements);

	void interleave(void* dst, glm::vec3* vertices, glm::vec2* tx_coord, glm::vec3* normals, glm::vec4* tangents, int elements);

	int get_binary_file_content(std::string &&file_path, std::vector<uint8_t>& file_binary_content);

	uint32_t get_buffer_image_alignment(uint64_t start_of_memory_binding, uint32_t image_alignment);

	uint32_t get_aligned_memory_size(VkMemoryRequirements m_r);
	uint32_t get_aligned_memory_size(VkMemoryRequirements m_r, uint32_t alignment);
	uint32_t get_alignment_memory(uint64_t mem_size, uint32_t alignment);

	int copy_gltf_contents(tinygltf::Model &model,
                           const std::vector<vulkan_helper::v_model_attributes> &v_attributes_to_copy,
						   bool vertex_normalize,
						   bool index_resolve,
						   const std::vector<vulkan_helper::t_model_attributes> &t_attributes_to_copy,
                           void *dst_ptr, vulkan_helper::model_data_info &out_data);

	uint64_t get_model_data_total_size(const model_data_info &model);
    
}