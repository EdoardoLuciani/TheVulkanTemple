#ifndef BASE_VULKAN_APP_GLTF_MODEL_H
#define BASE_VULKAN_APP_GLTF_MODEL_H

#include <string>
#include <array>
#include <glm/glm.hpp>
#include "external/tiny_gltf.h"
#include "vk_model.h"
#include "external/volk.h"

class GltfModel {
    public:
        static constexpr int v_model_attributes_max_set_bits = 4;
        enum v_model_attributes {
            V_VERTEX = 1, //00000001
            V_TEX_COORD = 2,//00000010
            V_NORMAL = 4, //00000100
            V_TANGENT = 8, //000001000
            V_ALL = 15 //000001111
        };

        static constexpr int t_model_attributes_max_set_bits = 4;
        enum t_model_attributes {
            T_ALBEDO_MAP = 1,
            T_ORM_MAP = 2,
            T_NORMAL_MAP = 4,
            T_EMISSIVE_MAP = 8,
            T_ALL = 15
        };

        enum class gltf_errors {
            LOADING_FAILED
        };

        // The model in order to be processed correctly should have:
        // - Same number of vertex, tangents, texture coordinates and normals
        // - Equal size for every image
        // - Only one set of texture coordinates (TEXCOORD_0)
        // - Only one material
        // - Only one mesh
        // - Only one buffer
        GltfModel() = default;
        GltfModel(std::string model_path);
		std::vector<VkModel::primitive_host_data_info> copy_model_data_in_ptr(uint8_t v_attributes_to_copy, bool vertex_normalize, bool index_resolve, uint8_t t_attributes_to_copy, void *dst_ptr);

    private:
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;

        struct geometry_attribute {
            uint32_t byte_offset = 0;
            uint32_t byte_lenght = 0;
            uint32_t element_count = 0;
            uint8_t element_size = 0;
        };

        struct map_attribute {
            int32_t index;
            glm::uvec2 size;
        };

        struct attributes {
			// 0 for POSITION
			// 1 for TEXCOORD_0
			// 2 for NORMAL
			// 3 for TANGENT
			std::unordered_map<std::string, geometry_attribute> geom_attributes;

			// 4 for indices
			geometry_attribute index_attributes;

			// 0 albedo_map
			// 1 orm_map_index
			// 2 normal_map_index
			// 3 emissive_map_index;
			std::array<map_attribute, 4> maps;
        };
        std::vector<attributes> primitive_attributes;

        void normalize_positions();
};
#endif //BASE_VULKAN_APP_GLTF_MODEL_H
