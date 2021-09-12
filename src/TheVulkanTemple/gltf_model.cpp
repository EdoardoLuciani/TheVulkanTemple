#include "gltf_model.h"
#include <unordered_map>
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <glm/gtx/norm.hpp>
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

		// We get the indices of the textures, then if they exist (!= 1) we convert them to images indices
		int mat_index = model.meshes[0].primitives[i].material;

		primitive_attributes[i].maps[0].index = model.materials[mat_index].pbrMetallicRoughness.baseColorTexture.index;
		primitive_attributes[i].maps[1].index = model.materials[mat_index].pbrMetallicRoughness.metallicRoughnessTexture.index;
		primitive_attributes[i].maps[2].index = model.materials[mat_index].normalTexture.index;
		primitive_attributes[i].maps[3].index = model.materials[mat_index].emissiveTexture.index;

		for (uint32_t j = 0; j < primitive_attributes[i].maps.size(); j++) {
			if (primitive_attributes[i].maps[j].index != -1) {
				primitive_attributes[i].maps[j].index = model.textures[primitive_attributes[i].maps[j].index].source;

				primitive_attributes[i].maps[j].size.x = model.images[primitive_attributes[i].maps[j].index].width;
				primitive_attributes[i].maps[j].size.y = model.images[primitive_attributes[i].maps[j].index].height;
			}
		}
    }
}

std::vector<VkModel::primitive_host_data_info> GltfModel::copy_model_data_in_ptr(uint8_t v_attributes_to_copy, bool vertex_normalize, bool index_resolve, uint8_t t_attributes_to_copy, void *dst_ptr,
                                                                                 bool compute_bounding_spheres) {
    std::vector<VkModel::primitive_host_data_info> last_copied_data_infos(primitive_attributes.size());
    // Clear all contents of the vector
    memset(last_copied_data_infos.data(), 0, last_copied_data_infos.size()*sizeof(VkModel::primitive_host_data_info));

    // Normalize vectors on request
    if (vertex_normalize) {
    	this->normalize_positions();
    }

    if (compute_bounding_spheres) {
        this->compute_all_primitives_bounding_spheres(last_copied_data_infos);
    }

    uint32_t written_data_size = 0;
	for (uint32_t i = 0; i < primitive_attributes.size(); i++) {
		// Calculate the block size for each point and store the vertices based on attribute request and availability
		uint32_t predicted_data_size = 0;
		std::array<std::string,4> map_indices = {"POSITION", "TEXCOORD_0", "NORMAL", "TANGENT"};
		uint32_t group_size = 0;
		for (uint32_t j = 0; j < map_indices.size(); j++) {
			if (v_attributes_to_copy & (1 << j)) {
				group_size += primitive_attributes[i].geom_attributes[map_indices[j]].element_size;
			}
		}
		last_copied_data_infos[i].vertices = primitive_attributes[i].geom_attributes["POSITION"].element_count;
		last_copied_data_infos[i].interleaved_vertices_data_size = group_size * last_copied_data_infos[i].vertices;
		predicted_data_size += last_copied_data_infos[i].interleaved_vertices_data_size;

		if (index_resolve) {
			last_copied_data_infos[i].index_data_size = primitive_attributes[i].index_attributes.byte_lenght;
			last_copied_data_infos[i].indices = primitive_attributes[i].index_attributes.element_count;
			predicted_data_size += last_copied_data_infos[i].index_data_size;
		}

		// The image before copying needs to be placed in an address which should be aligned within the image format case i.e. VK_FORMAT_R8G8B8A8_* = 4
		if (t_attributes_to_copy != 0) {
			last_copied_data_infos[i].image_alignment_size = vulkan_helper::get_alignment_memory(predicted_data_size, 4);
			predicted_data_size += last_copied_data_infos[i].image_alignment_size;
		}

		for (uint8_t j = 0; j < t_model_attributes_max_set_bits; j++) {
			if ((t_attributes_to_copy & (1 << j)) && (primitive_attributes[i].maps[j].index != -1)) {
				last_copied_data_infos[i].image_extent = { primitive_attributes[i].maps[j].size.x, primitive_attributes[i].maps[j].size.y, 1};
				last_copied_data_infos[i].image_layers++;
				predicted_data_size += last_copied_data_infos[i].get_texture_size();
			}
		}

		if (dst_ptr != nullptr) {
			// Interleaved vertex data copy
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

			if (t_attributes_to_copy !=0) {
				// Image needs to be aligned by the size of its format
				written_data_size += last_copied_data_infos[i].image_alignment_size;
			}

			for (uint8_t j=0; j < t_model_attributes_max_set_bits; j++) {
				if ((t_attributes_to_copy & (1 << j)) && (primitive_attributes[i].maps[j].index != -1)) {
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

void GltfModel::normalize_positions() {
	float max_len = std::numeric_limits<float>::min();
	for (const auto& attrib : primitive_attributes) {
		for (uint32_t i = 0; i < attrib.geom_attributes.at("POSITION").element_count; i++) {
			float vec_length = glm::length(*reinterpret_cast<glm::vec3*>(attrib.geom_attributes.at("POSITION").element_size*i +
					attrib.geom_attributes.at("POSITION").byte_offset + model.buffers[0].data.data()));
			if (vec_length > max_len) {
				max_len = vec_length;
			}
		}
	}

	for (const auto& attrib : primitive_attributes) {
		for (uint32_t i = 0; i < attrib.geom_attributes.at("POSITION").element_count; i++) {
			*reinterpret_cast<glm::vec3*>(attrib.geom_attributes.at("POSITION").element_size*i +
			attrib.geom_attributes.at("POSITION").byte_offset + model.buffers[0].data.data()) /= max_len;
		}
	}
}

void GltfModel::compute_all_primitives_bounding_spheres(std::vector<VkModel::primitive_host_data_info> &infos) {
    for (uint32_t i = 0; i < infos.size(); i++) {
        infos[i].b_sphere = this->compute_bounding_sphere(reinterpret_cast<glm::vec3*>(primitive_attributes[i].geom_attributes.at("POSITION").byte_offset + model.buffers[0].data.data()),
                                                          primitive_attributes[i].geom_attributes.at("POSITION").element_count);
    }
}

VkModel::primitive_host_data_info::bounding_sphere GltfModel::compute_bounding_sphere(glm::vec3 *vertices, uint64_t vertices_count) {
    VkModel::primitive_host_data_info::bounding_sphere return_sphere;
    float mRadius;
    float mRadius2;

    glm::vec3 xmin(std::numeric_limits<float>::max());
    glm::vec3 xmax(std::numeric_limits<float>::min());
    glm::vec3 ymin(std::numeric_limits<float>::max());
    glm::vec3 ymax(std::numeric_limits<float>::min());
    glm::vec3 zmin(std::numeric_limits<float>::max());
    glm::vec3 zmax(std::numeric_limits<float>::min());
    glm::vec3 dia1;
    glm::vec3 dia2;

    for (uint64_t i = 0; i < vertices_count; i++) {
        if (vertices[i][0] < xmin[0])
            xmin = vertices[i];
        if (vertices[i][0] > xmax[0])
            xmax = vertices[i];
        if (vertices[i][1] < ymin[1])
            ymin = vertices[i];
        if (vertices[i][1] > ymax[1])
            ymax = vertices[i];
        if (vertices[i][2] < zmin[2])
            zmin = vertices[i];
        if (vertices[i][2] > zmax[2])
            zmax = vertices[i];
    }

    /* Set xspan = distance between the 2 points xmin & xmax (squared) */
    glm::vec3 delta = xmax - xmin;
    float xspan = glm::dot(delta, delta);

    /* Same for y & z spans */
    delta = ymax - ymin;
    float yspan = glm::dot(delta, delta);

    delta = zmax - zmin;
    float zspan = glm::dot(delta, delta);

    /* Set points dia1 & dia2 to the maximally separated pair */
    dia1 = xmin;
    dia2 = xmax;

    /* assume xspan biggest */
    float maxspan = xspan;

    if (yspan > maxspan) {
        maxspan = yspan;
        dia1 = ymin;
        dia2 = ymax;
    }

    if (zspan > maxspan) {
        dia1 = zmin;
        dia2 = zmax;
    }

    /* dia1,dia2 is a diameter of initial sphere */
    /* calc initial center */
    return_sphere.center = (dia1 + dia2) * 0.5f;

    /* calculate initial radius**2 and radius */
    delta = dia2 - return_sphere.center;

    mRadius2 = glm::dot(delta, delta);
    mRadius = std::sqrt(mRadius2);

    /* SECOND PASS: increment current sphere */
    for (uint64_t i = 0; i < vertices_count; i++) {
        delta = vertices[i] - return_sphere.center;

        float old_to_p_sq = glm::dot(delta, delta);
        if (old_to_p_sq > mRadius2) /* do r**2 test first */{                           /* this point is outside of current sphere */
            float old_to_p = float(sqrt(old_to_p_sq));
            /* calc radius of new sphere */
            mRadius = (mRadius + old_to_p) * 0.5f;
            mRadius2 = mRadius * mRadius; /* for next r**2 compare */
            float old_to_new = old_to_p - mRadius;
            /* calc center of new sphere */
            float recip = 1.0f / old_to_p;
            return_sphere.center = (mRadius * return_sphere.center + old_to_new *vertices[i]) * recip;
        }
    }
    return_sphere.radius = mRadius;
    return return_sphere;
}
