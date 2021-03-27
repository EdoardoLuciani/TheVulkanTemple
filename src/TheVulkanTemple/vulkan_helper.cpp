#include "vulkan_helper.h"
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <bitset>
#include <cstring>

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

    VkBool32 compare_device_features_struct(const void* base, const void* requested, uint32_t size) {
        uint8_t offset = sizeof(int) + sizeof(void*);

        const VkBool32 *base_str_ptr = reinterpret_cast<const VkBool32*>(reinterpret_cast<const uint8_t*>(base) + offset);
        const VkBool32 *requested_str_ptr = reinterpret_cast<const VkBool32*>(reinterpret_cast<const uint8_t*>(requested) + offset);

        uint32_t corrected_size = (size - offset)/sizeof(VkBool32);
        for(size_t i = 0; i < corrected_size; i++) {
            if (base_str_ptr[i] == VK_FALSE && requested_str_ptr[i] == VK_TRUE) {
                return VK_FALSE;
            }
        }
        return VK_TRUE;
    }

    void* create_device_feature_struct_chain(const std::vector<const char*> &device_extensions) {
        void *first = nullptr;
        void *last = nullptr;

        for (const auto &device_extension : device_extensions) {
            if (!strcmp(device_extension,"VK_EXT_descriptor_indexing")) {
                VkPhysicalDeviceDescriptorIndexingFeaturesEXT* str = new VkPhysicalDeviceDescriptorIndexingFeaturesEXT;
                str->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
                str->pNext = nullptr;

                if (first == nullptr) {
                    first = str;
                }
                else {
                    // Even though the precendent type is not really this one we use it anyway
                    reinterpret_cast<VkPhysicalDeviceFeatures2*>(last)->pNext = str;
                }
                last = str;

            }
            else if (!strcmp(device_extension,"VK_KHR_shader_float16_int8")) {
                VkPhysicalDeviceShaderFloat16Int8FeaturesKHR* str = new VkPhysicalDeviceShaderFloat16Int8FeaturesKHR;
                str->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
                str->pNext = nullptr;

                if (first == nullptr) {
                    first = str;
                }
                else {
                    // Even though the precendent type is not really this one we use it anyway
                    reinterpret_cast<VkPhysicalDeviceFeatures2*>(last)->pNext = str;
                }
                last = str;

            }
        }
        return first;
    }

    VkBool32 compare_device_feature_struct_chain(void const *base, void const *requested) {
        void const *p_next_base = base;
        void const *p_next_requested = requested;

        while (p_next_base != nullptr && p_next_requested != nullptr) {
            switch(*reinterpret_cast<const int*>(p_next_base)) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT:
                    if (!compare_device_features_struct(p_next_base, p_next_requested, sizeof(VkPhysicalDeviceDescriptorIndexingFeaturesEXT))) {
                        return VK_FALSE;
                    }

                    p_next_base = reinterpret_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT const*>(p_next_base)->pNext;
                    p_next_requested = reinterpret_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT const*>(p_next_requested)->pNext;
                    break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR:
                    if (!compare_device_features_struct(p_next_base, p_next_requested, sizeof(VkPhysicalDeviceShaderFloat16Int8FeaturesKHR))) {
                        return VK_FALSE;
                    }

                    p_next_base = reinterpret_cast<VkPhysicalDeviceShaderFloat16Int8FeaturesKHR const*>(p_next_base)->pNext;
                    p_next_requested = reinterpret_cast<VkPhysicalDeviceShaderFloat16Int8FeaturesKHR const*>(p_next_requested)->pNext;
                    break;
            }
        }
        return (p_next_base == nullptr && p_next_requested == nullptr) ? VK_TRUE : VK_FALSE;
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

    void check_error(int32_t last_return_value, Error error_location) {
        if (last_return_value) {
            throw std::make_pair(last_return_value, error_location);
        }
    }

    void insert_or_sum(std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> &target, const std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> &to_sum) {
        for (auto& elem : to_sum.first) {
            if (!target.first.insert(elem).second) {
                target.first[elem.first] += elem.second;
            }
        }
        target.second += to_sum.second;
    }

    std::vector<VkDescriptorPoolSize> convert_map_to_vector(const std::unordered_map<VkDescriptorType, uint32_t> &target) {
        std::vector<VkDescriptorPoolSize> out_vec;
        out_vec.reserve(target.size());
        for (auto& elem : target) {
            out_vec.push_back({elem.first, elem.second});
        }
        return out_vec;
    }

    uint32_t get_mipmap_count(VkExtent3D image_extent) {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(image_extent.width, image_extent.height)))) + 1;
    }

} // namespace vulkan_helper
