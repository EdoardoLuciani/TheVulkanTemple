#include "gltf_model.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include <array>

GltfModel::GltfModel(std::string model_path, glm::mat4 model_matrix) {
    this->set_model_matrix(model_matrix);

    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, model_path)) {
        throw gltf_errors::LOADING_FAILED;
    }

    // We need the strings in order to access each attribute
    std::array<const char*,4> v_model_attributes_string {
            "POSITION",
            "TEXCOORD_0",
            "NORMAL",
            "TANGENT"
    };

    // We set the element size for each attribute, index will receive its element size later on
    attributes[0].element_size = 12; // vec3
    attributes[1].element_size = 8; // vec2
    attributes[2].element_size = 12; // vec3
    attributes[3].element_size = 16; // vec4

    // We read the data from the file and store it
    for (uint8_t i=0; i < v_model_attributes_string.size(); i++) {
        auto it = model.meshes[0].primitives[0].attributes.find(v_model_attributes_string[i]);
            if (it != model.meshes[0].primitives[0].attributes.end()) {
                attributes[i].byte_offset = model.accessors[it->second].byteOffset;
                attributes[i].byte_offset += model.bufferViews[model.accessors[it->second].bufferView].byteOffset;
                attributes[i].byte_lenght = model.bufferViews[model.accessors[it->second].bufferView].byteLength;
                attributes[i].element_count = attributes[i].byte_lenght / attributes[i].element_size;
        }
    }

    // We read the indices data and store it
    int acc_indices = model.meshes[0].primitives[0].indices;
    if (acc_indices != -1) {
        attributes[4].byte_offset = model.accessors[acc_indices].byteOffset;
        attributes[4].byte_offset += model.bufferViews[model.accessors[acc_indices].bufferView].byteOffset;
        attributes[4].byte_lenght = model.bufferViews[model.accessors[acc_indices].bufferView].byteLength;
        attributes[4].element_size = model.accessors[acc_indices].componentType - 5121;
        attributes[4].element_count = attributes[4].byte_lenght / attributes[4].element_size;
    }

    // We get the indices of the textures and then we take their sizes
    maps[0].index = model.materials[0].pbrMetallicRoughness.baseColorTexture.index;
    maps[1].index = model.materials[0].pbrMetallicRoughness.metallicRoughnessTexture.index;
    maps[2].index = model.materials[0].normalTexture.index;
    maps[3].index = model.materials[0].emissiveTexture.index;

    for (int i = 0; i < maps.size(); i++) {
        if (maps[i].index != -1) {
            maps[i].size.x = model.images[maps[i].index].width;
            maps[i].size.y = model.images[maps[i].index].height;
        }
    }

    for (int i = 0; i < maps.size(); i++) {
        if (maps[i].index != -1) {
            maps[i].size.x = model.images[maps[i].index].width;
            maps[i].size.y = model.images[maps[i].index].height;
        }
    }
}

GltfModel::copied_data_info GltfModel::copy_model_data_in_ptr(uint8_t v_attributes_to_copy, bool vertex_normalize, bool index_resolve, uint8_t t_attributes_to_copy, void *dst_ptr) {
    memset(&last_copied_data_info, 0, sizeof(copied_data_info));

    // Normalize vectors on request
    if (vertex_normalize && attributes[0].byte_lenght && dst_ptr != nullptr) {
        normalize_vectors(reinterpret_cast<glm::vec3*>(model.buffers[0].data.data() + attributes[0].byte_offset), attributes[0].element_count);
    }

    // Calculate the block size for each point and store the vertices based on attribute request and availability
    int group_size = 0;
    for (uint8_t i=0; i<v_model_attributes_max_set_bits; i++) {
        if (v_attributes_to_copy & (1 << i)) {
            if (attributes[i].element_count) {
                last_copied_data_info.vertices = attributes[i].element_count;
            }
            group_size += attributes[i].element_size;
        }
    }

    last_copied_data_info.interleaved_mesh_data_size = group_size * last_copied_data_info.vertices;

    // Vertex data copy
    if (dst_ptr != nullptr) {
        for (uint32_t i = 0; i < attributes[0].element_count; i++) {
            int written_data_size = 0;
            for (uint8_t j=0; j<v_model_attributes_max_set_bits; j++) {
                if (v_attributes_to_copy & (1 << j)) {
                    memcpy(static_cast<uint8_t *>(dst_ptr) + (i * group_size) + written_data_size,
                           model.buffers[0].data.data() + attributes[j].byte_offset + i * attributes[j].element_size,
                           attributes[j].element_size);
                    written_data_size += attributes[j].element_size;
                }
            }
        }
    }

    if (index_resolve) {
        last_copied_data_info.index_data_size = attributes[4].byte_lenght;
        last_copied_data_info.indices = attributes[4].element_count;
        last_copied_data_info.index_data_type = static_cast<VkIndexType>(attributes[4].element_size - (attributes[4].element_size - 1) - 1);

        if (dst_ptr != nullptr) {
            for (uint32_t i = 0; i < attributes[4].element_count; i++) {
                memcpy(static_cast<uint8_t *>(dst_ptr) + last_copied_data_info.interleaved_mesh_data_size,
                       model.buffers[0].data.data() + attributes[4].byte_offset,
                       attributes[4].byte_lenght);
            }
        }
    }

    int progressive_data = 0;
    for (uint8_t i=0; i<t_model_attributes_max_set_bits; i++) {
        if (t_attributes_to_copy & (1 << i)) {
            if (dst_ptr != nullptr) {
                memcpy(static_cast<uint8_t *>(dst_ptr) + last_copied_data_info.interleaved_mesh_data_size + last_copied_data_info.index_data_size + progressive_data,
                       model.images[maps[i].index].image.data(),
                       model.images[maps[i].index].image.size());
                progressive_data += model.images[maps[i].index].image.size();
            }
            last_copied_data_info.image_size = {maps[i].size.x, maps[i].size.y, 1};
            last_copied_data_info.image_layers++;
        }
    }

    copied_data_info boi;
    return boi;
}

uint64_t GltfModel::get_last_copy_total_size() const {
    int size_per_texel = 4;
    return last_copied_data_info.interleaved_mesh_data_size + last_copied_data_info.index_data_size +
            last_copied_data_info.image_size.width * last_copied_data_info.image_size.height * last_copied_data_info.image_size.depth * size_per_texel * last_copied_data_info.image_layers;
}

uint64_t GltfModel::get_last_copy_mesh_and_index_data_size() const {
    return last_copied_data_info.interleaved_mesh_data_size + last_copied_data_info.index_data_size;
}

uint64_t GltfModel::get_last_copy_texture_size() const {
    int size_per_texel = 4;
    return last_copied_data_info.image_size.width * last_copied_data_info.image_size.height * last_copied_data_info.image_size.depth * size_per_texel * last_copied_data_info.image_layers;
}

void GltfModel::set_model_matrix(glm::mat4 model_matrix) {
    this->model_matrix = model_matrix;
    this->normal_matrix = glm::transpose(glm::inverse(model_matrix));
}

uint32_t GltfModel::copy_uniform_data(uint8_t *dst_ptr) {
    if (dst_ptr != nullptr) {
        memcpy(dst_ptr, &model_matrix, sizeof(glm::mat4));
        memcpy(dst_ptr + sizeof(glm::mat4), &normal_matrix, sizeof(glm::mat4));
    }
    return sizeof(glm::mat4)*2;
}

void GltfModel::normalize_vectors(glm::vec3 *vectors, int number_of_elements) {
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