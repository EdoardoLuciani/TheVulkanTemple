#ifndef THEVULKANTEMPLE_VK_MODEL_H
#define THEVULKANTEMPLE_VK_MODEL_H

#include "vulkan_helper.h"
#include <span>
#include "external/vk_mem_alloc.h"

// Class that manages and acts on a model from a vulkan perspective,
// note: only the images are managed by this class. The vertices attributes (position, texcoords, normals and tangents)
// are managed by the parent class
// TODO: implement a class to suballocate vertices data in a buffer
class VkModel {
	public:
		struct primitive_host_data_info {
			uint32_t interleaved_vertices_data_size;
			uint32_t vertices;

			uint32_t index_data_size;
			uint32_t indices;

			uint32_t image_alignment_size;
			VkExtent3D image_extent;
			uint32_t image_layers;

			uint64_t get_total_size() const {
				return get_mesh_and_index_data_size() + image_alignment_size + get_texture_size();
			}

			uint64_t get_mesh_and_index_data_size() const {
				return this->interleaved_vertices_data_size + this->index_data_size;
			}

			uint64_t get_texture_size() const {
				return this->image_extent.width * this->image_extent.height * this->image_extent.depth * 4 * image_layers; // size per texel
			}
		};

		struct primitive_device_data_info {
			VkImage image = VK_NULL_HANDLE;
			VmaAllocation allocation = VK_NULL_HANDLE;
			VkImageView image_view = VK_NULL_HANDLE;
			VkSampler sampler =  VK_NULL_HANDLE;

			VkBuffer data_buffer;
			VkDescriptorSet descriptor_set;
			uint64_t index_data_offset;
			VkIndexType index_data_type;
			uint64_t primitive_vertices_data_offset;
		};

		VkModel(VkDevice device, std::string model_file_path, std::vector<primitive_host_data_info> infos, glm::mat4 model_matrix = glm::mat4(1.0f));
		~VkModel();

		uint64_t get_all_primitives_total_size() const;
		uint64_t get_all_primitives_mesh_and_indices_size() const;

		void set_model_matrix(glm::mat4 model_matrix);
		uint32_t copy_uniform_data(uint8_t *dst_ptr) const;

        // Creating the image, image view and sampler of each primitive in the model
        void vk_create_images(float mip_bias, VmaAllocator vma_allocator);

        // copies mesh data from host buffer to device buffer and copies image data from host buffer to device image
        void vk_init_model(VkCommandBuffer cb, VkBuffer host_buffer, uint64_t host_buffer_offset, VkBuffer device_buffer, uint64_t device_buffer_offset);

		// By giving the descriptor sets (with .size == primitives) it returns the structures to pass to vkWriteDescriptorSets
		std::vector<VkWriteDescriptorSet> get_descriptor_writes(std::span<VkDescriptorSet> descriptor_sets, VkBuffer uniform_buffer, uint32_t uniform_buffer_offset);

		// The vector returned by get_descriptor_writes has pointers inside to dynamically allocated memory, this function cleans them
		void clean_descriptor_writes(std::span<VkWriteDescriptorSet> first_two_write_descriptor_set);

		// Before recording the draw, all fields of device_data_info needs to be set
		void vk_record_draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t model_set_shader_index) const;
	private:

        // Copies data from a host buffer to the images and creates all mipmaps level from them
        uint64_t vk_init_images(VkCommandBuffer cb, VkBuffer host_image_transient_buffer, uint64_t primitive_host_buffer_offset);

        // copies the mesh data from a host buffer to a device buffer
        void vk_record_buffer_copies_from_host_to_device(VkCommandBuffer cb, VkBuffer host_buffer, uint64_t host_buffer_offset, VkBuffer device_buffer, uint64_t device_buffer_offset);

		std::string model_file_path;
		VkDevice device;
		VmaAllocator vma_allocator;
		glm::mat4 model_matrix = glm::mat4(1.0f);
		glm::mat4 normal_matrix = glm::mat4(1.0f);

		std::vector<primitive_host_data_info> host_primitives_data_info;
		std::vector<primitive_device_data_info> device_primitives_data_info;

		friend class GraphicsModuleVulkanApp;
};

#endif
