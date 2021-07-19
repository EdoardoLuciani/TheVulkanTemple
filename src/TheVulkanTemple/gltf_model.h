#ifndef BASE_VULKAN_APP_GLTF_MODEL_H
#define BASE_VULKAN_APP_GLTF_MODEL_H

#include <string>
#include <array>
#include <glm/glm.hpp>
#include "external/tiny_gltf.h"
#include "volk.h"

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

        struct copied_data_info {
            uint32_t interleaved_mesh_data_size;
            uint32_t vertices;

            uint32_t index_data_size;
            uint32_t indices;
            VkIndexType index_data_type;

            VkExtent3D image_size;
            uint32_t image_layers;
        };

        // The model in order to be processed correctly should have:
        // - Same number of vertex, tangents, texture coordinates and normals
        // - Equal size for every image
        // - Only one set of texture coordinates (TEXCOORD_0)
        // - Only one material
        // - Only one mesh
        // - Only one buffer
        GltfModel() {};
        GltfModel(std::string model_path, glm::mat4 matrix);
        copied_data_info copy_model_data_in_ptr(uint8_t v_attributes_to_copy, bool vertex_normalize, bool index_resolve, uint8_t t_attributes_to_copy, void *dst_ptr);

        uint64_t get_last_copy_total_size() const;
        uint64_t get_last_copy_mesh_and_index_data_size() const;
        uint64_t get_last_copy_texture_size() const;

        uint32_t get_last_copy_mesh_size() const { return last_copied_data_info.interleaved_mesh_data_size; };
        uint32_t get_last_copy_vertices() const { return last_copied_data_info.vertices; };
        uint32_t get_last_copy_index_size() const { return last_copied_data_info.index_data_size; };
        uint32_t get_last_copy_indices() const { return last_copied_data_info.indices; };
        VkIndexType get_last_copy_index_type() const { return last_copied_data_info.index_data_type; };
        VkExtent3D get_last_copy_image_extent() const { return last_copied_data_info.image_size; };
        uint32_t get_last_copy_image_layers() const { return last_copied_data_info.image_layers; };

        void set_model_matrix(glm::mat4 model_matrix);
        uint32_t copy_uniform_data(uint8_t *dst_ptr);

    private:
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;

        struct attribute {
            uint32_t byte_offset = 0;
            uint32_t byte_lenght = 0;
            uint32_t element_count = 0;
            uint8_t element_size = 0;
        };
        // 0 for POSITION
        // 1 for TEXCOORD_0
        // 2 for NORMAL
        // 3 for TANGENT
        // 4 for indices
        std::array<attribute, 5> attributes;

        struct map_attribute {
            int32_t index;
            glm::uvec2 size;
        };
        // 0 albedo_map
        // 1 orm_map_index
        // 2 normal_map_index
        // 3 emissive_map_index;
        std::array<map_attribute, 4> maps;

        copied_data_info last_copied_data_info;

        glm::mat4 model_matrix;
        glm::mat4 normal_matrix;

        void normalize_vectors(glm::vec3 * vectors, int number_of_elements);
};

struct vk_object_render_info {
    GltfModel model;
    VkBuffer data_buffer;
    uint64_t mesh_data_offset;
    uint64_t index_data_offset;
};
#endif //BASE_VULKAN_APP_GLTF_MODEL_H
