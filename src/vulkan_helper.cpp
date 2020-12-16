#include "vulkan_helper.h"
#include <iostream>
#include <filesystem>
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <bitset>

namespace vulkan_helper
{

    VkPresentModeKHR select_presentation_mode(const std::vector<VkPresentModeKHR> &presentation_modes, VkPresentModeKHR desired_presentation_mode) {
        VkPresentModeKHR selected_present_mode;
        if (std::find(presentation_modes.begin(), presentation_modes.end(), desired_presentation_mode) != presentation_modes.end()) {
            selected_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
        else {
            selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        }
        return selected_present_mode;
    }

    uint32_t select_number_of_images(const VkSurfaceCapabilitiesKHR &surface_capabilities) {
        uint32_t number_of_images = surface_capabilities.minImageCount + 1;
        if ((surface_capabilities.maxImageCount > 0) && (number_of_images > surface_capabilities.maxImageCount)) {
            number_of_images = surface_capabilities.maxImageCount;
        }
        return number_of_images;
    }

    VkExtent2D select_size_of_images(const VkSurfaceCapabilitiesKHR &surface_capabilities, VkExtent2D desired_size_of_images) {
        if (0xFFFFFFFF == surface_capabilities.currentExtent.width) {
            if (desired_size_of_images.width < surface_capabilities.minImageExtent.width) {
                desired_size_of_images.width = surface_capabilities.minImageExtent.width;
            }
            else if (desired_size_of_images.width > surface_capabilities.maxImageExtent.width) {
                desired_size_of_images.width = surface_capabilities.maxImageExtent.width;
            }

            if (desired_size_of_images.height < surface_capabilities.minImageExtent.height) {
                desired_size_of_images.height = surface_capabilities.minImageExtent.height;
            }
            else if (desired_size_of_images.height > surface_capabilities.maxImageExtent.height) {
                desired_size_of_images.height = surface_capabilities.maxImageExtent.height;
            }
        }
        else {
            desired_size_of_images = surface_capabilities.currentExtent;
        }
        return desired_size_of_images;
    }

    VkImageUsageFlags select_image_usage(const VkSurfaceCapabilitiesKHR &surface_capabilities, VkImageUsageFlags desired_usages) {
        VkImageUsageFlags image_usage;
        image_usage = desired_usages & surface_capabilities.supportedUsageFlags;
        if (desired_usages != image_usage) {
            image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        return image_usage;
    }

    VkSurfaceTransformFlagBitsKHR select_surface_transform(const VkSurfaceCapabilitiesKHR &surface_capabilities, VkSurfaceTransformFlagBitsKHR desired_transform) {
        VkSurfaceTransformFlagBitsKHR surface_transform;
        if (surface_capabilities.supportedTransforms & desired_transform) {
            surface_transform = desired_transform;
        }
        else {
            surface_transform = surface_capabilities.currentTransform;
        }
        return surface_transform;
    }

    VkSurfaceFormatKHR select_surface_format(const std::vector<VkSurfaceFormatKHR> &surface_formats, VkSurfaceFormatKHR desired_surface_format) {
        VkSurfaceFormatKHR selected_surface_format;
        if ((1 == surface_formats.size()) && (VK_FORMAT_UNDEFINED == surface_formats[0].format)) {
            selected_surface_format.format = desired_surface_format.format;
            selected_surface_format.colorSpace = desired_surface_format.colorSpace;
            return selected_surface_format;
        }

        for (auto &surface_format : surface_formats) {
            if ((desired_surface_format.format == surface_format.format) && (desired_surface_format.colorSpace == surface_format.colorSpace)) {
                selected_surface_format.format = desired_surface_format.format;
                selected_surface_format.colorSpace = desired_surface_format.colorSpace;
                return surface_format;
            }
        }

        for (auto &surface_format : surface_formats) {
            if (desired_surface_format.format == surface_format.format) {
                selected_surface_format.format = desired_surface_format.format;
                selected_surface_format.colorSpace = surface_format.colorSpace;
                return selected_surface_format;
            }
        }

        selected_surface_format = surface_formats[0];
        return selected_surface_format;
    }

    uint32_t select_memory_index(const VkPhysicalDeviceMemoryProperties &physical_device_memory_properties, const VkMemoryRequirements &memory_requirements, VkMemoryPropertyFlags memory_properties) {
        for (uint32_t type = 0; type < physical_device_memory_properties.memoryTypeCount; ++type) {
            if ((memory_requirements.memoryTypeBits & (1 << type)) &&
                ((physical_device_memory_properties.memoryTypes[type].propertyFlags & memory_properties) == memory_properties)) {
                return type;
            }
        }
        return VK_MAX_MEMORY_TYPES;
    }

    VkBool32 debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg, void *pUserData) {
        if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
            std::cerr << "ERROR: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
        }
        else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
            std::cerr << "WARNING: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
        }
        return VK_FALSE;
    }

    VkBool32 compare_physical_device_features_structs(VkPhysicalDeviceFeatures base, VkPhysicalDeviceFeatures requested) {
	    constexpr uint32_t feature_count = static_cast<uint32_t>(sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32));

    	std::array<VkBool32, feature_count> base_bools_arr;
	    std::memcpy(base_bools_arr.data(), &base, base_bools_arr.size()*sizeof(VkBool32));

    	std::array<VkBool32, feature_count> requested_bools_arr;
	    std::memcpy(requested_bools_arr.data(), &requested, requested_bools_arr.size()*sizeof(VkBool32));

	    for(size_t i = 0; i < base_bools_arr.size(); i++) {
		    if (base_bools_arr[i] == VK_FALSE && requested_bools_arr[i] == VK_TRUE) {
			    return VK_FALSE;
		    }
	    }
	    return VK_TRUE; 
    }

    void normalize_vectors(glm::vec3 *vectors, int number_of_elements) {
        float max_len = FLT_MIN;
        for (int i = 0; i < number_of_elements; i++) {
            if (glm::length(vectors[i]) > max_len) {
                max_len = glm::length(vectors[i]);
            }
        }

        for (int i = 0; i < number_of_elements; i++) {
            vectors[i].x = vectors[i].x / max_len;
            vectors[i].y = vectors[i].y / max_len;
            vectors[i].z = vectors[i].z / max_len;
        }
    }

    void interleave(void *dst, glm::vec3 *vertices, glm::vec2 *tx_coord, glm::vec3 *normals, glm::vec4 *tangents, int elements) {
        int group_size = 0;
        if (vertices != nullptr) {
            group_size += sizeof(glm::vec3);
        }
        if (tx_coord != nullptr) {
            group_size += sizeof(glm::vec2);
        }
        if (normals != nullptr) {
            group_size += sizeof(glm::vec3);
        }
        if (tangents != nullptr) {
            group_size += sizeof(glm::vec4);
        }

        int written_data_size;
        for (int i = 0; i < elements; i++) {
            written_data_size = 0;
            if (vertices != nullptr){
                memcpy(static_cast<uint8_t *>(dst) + (i * group_size), glm::value_ptr(vertices[i]), sizeof(glm::vec3));
                written_data_size = sizeof(glm::vec3);
            }

            if (tx_coord != nullptr) {
                memcpy(static_cast<uint8_t *>(dst) + (i * group_size + written_data_size), glm::value_ptr(tx_coord[i]), sizeof(glm::vec2));
                written_data_size += sizeof(glm::vec2);
            }
            if (normals != nullptr) {
                memcpy(static_cast<uint8_t *>(dst) + (i * group_size + written_data_size), glm::value_ptr(normals[i]), sizeof(glm::vec3));
                written_data_size += sizeof(glm::vec3);
            }
            if (tangents != nullptr) {
                memcpy(static_cast<uint8_t *>(dst) + (i * group_size + written_data_size), glm::value_ptr(tangents[i]), sizeof(glm::vec4));
                written_data_size += sizeof(glm::vec4);
            }
        }
    }

    int get_binary_file_content(const std::string &file_path, std::vector<uint8_t> &file_binary_content) {
        uint64_t file_size = std::filesystem::file_size(file_path);
        file_binary_content.resize(file_size);
        std::ifstream binary_file(file_path, std::ios::in | std::ios::binary);
        if (!binary_file.is_open()) {
            return 1;
        }
        binary_file.read(reinterpret_cast<char *>(file_binary_content.data()), file_size);
        binary_file.close();
        return 0;
    }

    int get_binary_file_content(const std::string &file_path, uint64_t &size, void *dst_ptr) {
        size = std::filesystem::file_size(file_path);
        if (dst_ptr == nullptr) {
            return 0;
        }
        std::ifstream binary_file(file_path, std::ios::in | std::ios::binary);
        if (!binary_file.is_open()) {
            return 1;
        }
        binary_file.read(static_cast<char*>(dst_ptr), size);
        binary_file.close();
        return 0;
    }

    uint32_t get_alignment_memory(uint64_t mem_size, uint32_t alignment) {
        return alignment * std::ceil(mem_size/static_cast<float>(alignment)) - mem_size;
    }

    uint32_t get_aligned_memory_size(uint64_t mem_size, uint32_t alignment) {
        return alignment * std::ceil(mem_size/static_cast<float>(alignment));
    }

    // The model in order to be processed correctly should have:
    // - Same number of vertex, tangents, texture coordinates and normals
    // - Equal size for every image
    // - Only one set of texture coordinates (TEXCOORD_0)
    // - Only one material
    // - Only one mesh
    // - Only one buffer

    // RETURN CODES:
    // 0: success
    // -1 v_attributes not all present in same quantities
    // -2 requested a not available v_attribute 
    // -3 index data not present in the model
    // -4 not all textures have the same size

    int copy_gltf_contents(tinygltf::Model &model,
                           uint8_t v_attributes_to_copy,
                           bool vertex_normalize,
                           bool index_resolve,
                           uint8_t t_attributes_to_copy,
                           void *dst_ptr, vulkan_helper::model_data_info &out_data) {

        std::array<uint32_t, 4> v_attr_n_elem;
        std::array<uint32_t, 4> v_attr_off;
        std::array<uint32_t, 4> v_attr_len;
        int32_t off_index = 0;

        // 0 albedo_map_index, 1 normal_map_index, 2 orm_map_index, 3 emissive_map_index;
        std::array<int32_t, 4> map_index;
        std::array<std::pair<uint32_t, uint32_t>, 4> map_size;

        memset(&out_data, 0, sizeof(model_data_info));

        std::array<const char*,4> v_model_attributes_string {
                "POSITION",
                "NORMAL",
                "TEXCOORD_0",
                "TANGENT"
        };

        std::array<int,4> v_model_attributes_sizes {
                12,
                12,
                8,
                16
        };

        for (uint8_t i=0; i<v_model_attributes_max_set_bits; i++) {
            if (v_attributes_to_copy & (1 << i)) {
                auto it = model.meshes[0].primitives[0].attributes.find(v_model_attributes_string[i]);
                if (it != model.meshes[0].primitives[0].attributes.end()) {
                    v_attr_off[i] = model.accessors[it->second].byteOffset;
                    v_attr_off[i] += model.bufferViews[model.accessors[it->second].bufferView].byteOffset;
                    v_attr_len[i] = model.bufferViews[model.accessors[it->second].bufferView].byteLength;
                    v_attr_n_elem[i] = v_attr_len[i] / v_model_attributes_sizes[i];
                    out_data.interleaved_mesh_data_size += v_attr_len[i];

                    // only the first time in the cycle
                    if (out_data.vertices == 0) {
                        out_data.vertices = v_attr_n_elem[i];
                    }
                    else if (out_data.vertices != v_attr_n_elem[i]) {
                        return -1;
                    }

                }
                else {
                    return -2;
                }
            }
        }

        if (index_resolve) {
            int acc_indices = model.meshes[0].primitives[0].indices;
            if (acc_indices != -1) {
                off_index = model.accessors[acc_indices].byteOffset;
                off_index += model.bufferViews[model.accessors[acc_indices].bufferView].byteOffset;
                out_data.index_data_size = model.bufferViews[model.accessors[acc_indices].bufferView].byteLength;
                if (model.accessors[acc_indices].componentType == 5123) {
                    out_data.index_data_type = VK_INDEX_TYPE_UINT16;
                }
                else if (model.accessors[acc_indices].componentType == 5125) {
                    out_data.index_data_type = VK_INDEX_TYPE_UINT32;
                }
                out_data.indices = out_data.index_data_size / (2 + out_data.index_data_type * 2);
            }
            else {
                return -3;
            }
        }

        for (uint8_t i=0; i<t_model_attributes_max_set_bits; i++) {
            switch (static_cast<t_model_attributes>(t_attributes_to_copy & (1 << i))) {
                case T_ALBEDO_MAP:
                    map_index[0] = model.materials[0].pbrMetallicRoughness.baseColorTexture.index;
                    break;
                case T_NORMAL_MAP:
                	map_index[1] = model.materials[0].normalTexture.index;
                    break;
                case T_ORM_MAP:
                    map_index[2] = model.materials[0].pbrMetallicRoughness.metallicRoughnessTexture.index;
                    break;
                case T_EMISSIVE_MAP:
                	map_index[3] = model.materials[0].emissiveTexture.index;
                    break;
                default:
                    continue;
            }
            if (map_index[i] != -1) {
                map_size[i].first = model.images[map_index[i]].width;
                map_size[i].second = model.images[map_index[i]].height;
                out_data.image_layers++;

                if (out_data.image_size.width == 0 && out_data.image_size.height == 0) {
                    out_data.image_size.width = map_size[i].first;
                    out_data.image_size.height = map_size[i].second;
                }
                else if (out_data.image_size.width != map_size[i].first || out_data.image_size.height != map_size[i].second) {
                    return -4;
                }
            }
        }
        out_data.image_size.depth = 1;

        // Parse only, no copy
        if (dst_ptr == nullptr) {
            return 0;
        }

        // Normalize vectors on request
        if (vertex_normalize && v_attr_len[0] ) {
            normalize_vectors(reinterpret_cast<glm::vec3*>(model.buffers[0].data.data() + v_attr_off[0]), v_attr_n_elem[0]);
        }

        // Calculate the block size for each vertex
        int group_size = 0;
        for (uint8_t i=0; i<v_model_attributes_max_set_bits; i++) {
        	if (t_attributes_to_copy & (1 << i)) {
		        group_size += v_model_attributes_sizes[i];
        	}
        }

        // Vertex data copy
        for (uint32_t i = 0; i < out_data.vertices; i++) {
            int written_data_size = 0;
	        for (uint8_t j=0; i<v_model_attributes_max_set_bits; i++) {
		        if (t_attributes_to_copy & (1 << j)) {
			        memcpy(static_cast<uint8_t *>(dst_ptr) + (i * group_size) + written_data_size,
			               model.buffers[0].data.data() + v_attr_off[j] + i * v_model_attributes_sizes[j],
			               v_model_attributes_sizes[j]);
			        written_data_size += v_model_attributes_sizes[j];
		        }
	        }
        }

        for (uint32_t i = 0; i < out_data.indices; i++) {
            memcpy(static_cast<uint8_t *>(dst_ptr) + out_data.interleaved_mesh_data_size,
                   model.buffers[0].data.data() + off_index,
                   out_data.index_data_size);
        }

        int progressive_data = 0;
        for (uint8_t i=0; i<t_model_attributes_max_set_bits; i++) {
	        if (t_attributes_to_copy & (1 << i)) {
		        memcpy(static_cast<uint8_t *>(dst_ptr) + out_data.interleaved_mesh_data_size + out_data.index_data_size + progressive_data,
		               model.images[map_index[i]].image.data(),
		               model.images[map_index[i]].image.size());
		        progressive_data += model.images[map_index[i]].image.size();
	        }
        }
        return 0;
    }

    uint64_t get_model_data_total_size(const model_data_info &model) {
        int size_per_texel = 4;
        return model.interleaved_mesh_data_size + model.index_data_size +
        model.image_size.width * model.image_size.height * model.image_size.depth * size_per_texel * model.image_layers;
    }

    uint64_t get_model_mesh_and_index_data_size(const model_data_info &model) {
        return model.interleaved_mesh_data_size + model.index_data_size;
    }

    uint64_t get_model_texture_size(const model_data_info &model) {
        int size_per_texel = 4;
        return model.image_size.width * model.image_size.height * model.image_size.depth * size_per_texel * model.image_layers;
    }

    void check_error(int32_t last_return_value, Error error_location) {
        if (last_return_value) {
            throw std::make_pair(last_return_value, error_location);
        }
    }

} // namespace vulkan_helper
