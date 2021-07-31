#include "gltf_model.h"
#include <unordered_map>
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/tiny_gltf.h"
#include <array>

GltfModel::GltfModel(std::string model_path) {
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, model_path)) {
        throw gltf_errors::LOADING_FAILED;
    }

    // We need the strings in order to access each attribute
    std::unordered_map<std::string, uint32_t> v_model_attributes_and_size {
			{"POSITION", 12},
			{"TEXCOORD_0", 8},
			{"NORMAL", 12},
			{"TANGENT", 16},
    };

    // We read the data from the file and store it
	primitive_attributes.resize(model.meshes[0].primitives.size());

    for (uint32_t i = 0; i < primitive_attributes.size(); i++) {
    	// Reading the attributes POSITION, TEXCOORD_0, NORMAL and TANGENT
		for (auto it = v_model_attributes_and_size.begin(); it != v_model_attributes_and_size.end(); it++) {
			auto attribute_accessor = model.meshes[0].primitives[i].attributes.find(it->first);
			if (attribute_accessor != model.meshes[0].primitives[i].attributes.end()) {
				primitive_attributes[i].geom_attributes[it->first].byte_offset = model.accessors[attribute_accessor->second].byteOffset;
				primitive_attributes[i].geom_attributes[it->first].byte_offset += model.bufferViews[model.accessors[attribute_accessor->second].bufferView].byteOffset;
				primitive_attributes[i].geom_attributes[it->first].byte_lenght = model.bufferViews[model.accessors[attribute_accessor->second].bufferView].byteLength;
				primitive_attributes[i].geom_attributes[it->first].element_count = primitive_attributes[i].geom_attributes[it->first].byte_lenght / it->second;
				primitive_attributes[i].geom_attributes[it->first].element_size = it->second;
			}
		}

		// Reading the indices data
		int acc_indices = model.meshes[0].primitives[i].indices;
		if (acc_indices != -1) {
			primitive_attributes[i].index_attributes.byte_offset = model.accessors[acc_indices].byteOffset;
			primitive_attributes[i].index_attributes.byte_offset += model.bufferViews[model.accessors[acc_indices].bufferView].byteOffset;
			primitive_attributes[i].index_attributes.byte_lenght = model.bufferViews[model.accessors[acc_indices].bufferView].byteLength;
			primitive_attributes[i].index_attributes.element_size = model.accessors[acc_indices].componentType - 5121;
			primitive_attributes[i].index_attributes.element_count = primitive_attributes[i].index_attributes.byte_lenght / primitive_attributes[i].index_attributes.element_size;
		}

		// We get the indices of the textures and then we take their sizes
		int mat_index = model.meshes[0].primitives[i].material;
		primitive_attributes[i].maps[0].index = model.materials[mat_index].pbrMetallicRoughness.baseColorTexture.index;
		primitive_attributes[i].maps[1].index = model.materials[mat_index].pbrMetallicRoughness.metallicRoughnessTexture.index;
		primitive_attributes[i].maps[2].index = model.materials[mat_index].normalTexture.index;
		primitive_attributes[i].maps[3].index = model.materials[mat_index].emissiveTexture.index;

		for (uint32_t j = 0; j < primitive_attributes[i].maps.size(); j++) {
			if (primitive_attributes[i].maps[j].index != -1) {
				primitive_attributes[i].maps[j].size.x = model.images[primitive_attributes[i].maps[i].index].width;
				primitive_attributes[i].maps[j].size.y = model.images[primitive_attributes[i].maps[i].index].height;
			}
		}
    }
}

std::vector<VkModel::primitive_host_data_info> GltfModel::copy_model_data_in_ptr(uint8_t v_attributes_to_copy, bool vertex_normalize, bool index_resolve, uint8_t t_attributes_to_copy, void *dst_ptr) {
    std::vector<VkModel::primitive_host_data_info> last_copied_data_infos(primitive_attributes.size());
    // Clear all contents of the vector
    memset(last_copied_data_infos.data(), 0, last_copied_data_infos.size()*sizeof(VkModel::primitive_host_data_info));

	for (uint32_t i = 0; i < primitive_attributes.size(); i++) {
		// Normalize vectors on request
		if (vertex_normalize && primitive_attributes[i].geom_attributes["POSITION"].byte_lenght && dst_ptr!=nullptr) {
			normalize_vectors(reinterpret_cast<glm::vec3*>(model.buffers[0].data.data()+primitive_attributes[i].geom_attributes["POSITION"].byte_offset),
					primitive_attributes[i].geom_attributes["POSITION"].element_count);
		}

		// Calculate the block size for each point and store the vertices based on attribute request and availability
		std::array<std::string,4> map_indices = {"POSITION", "TEXCOORD_0", "NORMAL", "TANGENT"};
		int group_size = 0;
		for (uint32_t j = 0; j < map_indices.size(); j++) {
			if (v_attributes_to_copy & (1 << j)) {
				group_size += primitive_attributes[i].geom_attributes[map_indices[j]].element_size;
			}
		}
		last_copied_data_infos[i].vertices = primitive_attributes[i].geom_attributes["POSITION"].element_count;
		last_copied_data_infos[i].interleaved_vertices_data_size = group_size * last_copied_data_infos[i].vertices;

		if (index_resolve) {
			last_copied_data_infos[i].index_data_size = primitive_attributes[i].index_attributes.byte_lenght;
			last_copied_data_infos[i].indices = primitive_attributes[i].index_attributes.element_count;
		}

		for (uint8_t j = 0; j < t_model_attributes_max_set_bits; j++) {
			if (t_attributes_to_copy & (1 << j)) {
				last_copied_data_infos[i].image_extent = { primitive_attributes[i].maps[j].size.x, primitive_attributes[i].maps[j].size.y, 1};
				last_copied_data_infos[i].image_layers++;
			}
		}

		// Interleaved vertex data copy
		if (dst_ptr != nullptr) {
			uint32_t written_data_size = 0;
			// all element count fields of POSITION, TEXCOORD_0, NORMAL and TANGENT *should* be the same
			for (uint32_t n_group = 0; n_group < primitive_attributes[i].geom_attributes["POSITION"].element_count; n_group++) {
				for (uint8_t k=0; k < v_model_attributes_max_set_bits; k++) {
					if (v_attributes_to_copy & (1 << k)) {
						memcpy(static_cast<uint8_t *>(dst_ptr) + written_data_size,
								model.buffers[0].data.data() + primitive_attributes[i].geom_attributes[map_indices[k]].byte_offset + n_group * primitive_attributes[i].geom_attributes[map_indices[k]].element_size,
								primitive_attributes[i].geom_attributes[map_indices[k]].element_size);
						written_data_size += primitive_attributes[i].geom_attributes[map_indices[k]].element_size;
					}
				}
			}

			if (index_resolve) {
				memcpy(static_cast<uint8_t *>(dst_ptr) + written_data_size,
					model.buffers[0].data.data() + primitive_attributes[i].index_attributes.byte_offset,
					primitive_attributes[i].index_attributes.byte_lenght);
				written_data_size += primitive_attributes[i].index_attributes.byte_lenght;
			}

			for (uint8_t j=0; j < t_model_attributes_max_set_bits; j++) {
				if (t_attributes_to_copy & (1 << j)) {
					memcpy(static_cast<uint8_t*>(dst_ptr) + written_data_size,
							model.images[primitive_attributes[i].maps[j].index].image.data(),
							model.images[primitive_attributes[i].maps[j].index].image.size());
					written_data_size += model.images[primitive_attributes[i].maps[j].index].image.size();
				}
			}
		}
	}
    return last_copied_data_infos;
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