#include "vk_model.h"

VkModel::VkModel(VkDevice device, std::string model_file_path, std::vector<primitive_host_data_info> infos, glm::mat4 model_matrix) :
		model_file_path{model_file_path}, device{device}, host_primitives_data_info{infos} {
	set_model_matrix(model_matrix);
	device_primitives_data_info.resize(host_primitives_data_info.size());
	for (uint32_t i = 0; i < device_primitives_data_info.size(); i++) {
		uint32_t index_data_type_size = host_primitives_data_info[i].index_data_size / host_primitives_data_info[i].indices;
		device_primitives_data_info[i].index_data_type = static_cast<VkIndexType>((index_data_type_size - 2)/2);
	}
}

VkModel::~VkModel() {
	for (uint32_t i=0; i < this->device_primitives_data_info.size(); i++) {
		vkDestroySampler(device, device_primitives_data_info[i].sampler, nullptr);
		vkDestroyImageView(device, device_primitives_data_info[i].image_view, nullptr);
		vmaDestroyImage(vma_allocator, device_primitives_data_info[i].image,  device_primitives_data_info[i].allocation);
	}
}

uint64_t VkModel::get_all_primitives_total_size() const {
	uint64_t total_size = 0;
	for (const auto& info : host_primitives_data_info) {
		total_size += info.get_total_size();
	}
	return total_size;
}

uint64_t VkModel::get_all_primitives_mesh_and_indices_size() const {
	uint64_t total_size = 0;
	for (const auto& info : host_primitives_data_info) {
		total_size += info.get_mesh_and_index_data_size();
	}
	return total_size;
}

void VkModel::set_model_matrix(glm::mat4 model_matrix) {
	this->model_matrix = model_matrix;
	this->normal_matrix = glm::transpose(glm::inverse(model_matrix));
}

uint32_t VkModel::copy_uniform_data(uint8_t *dst_ptr) const {
	if (dst_ptr != nullptr) {
		memcpy(dst_ptr, &model_matrix, sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4), &normal_matrix, sizeof(glm::mat4));
	}
	return sizeof(glm::mat4)*2;
}

void VkModel::vk_create_images(float mip_bias, VmaAllocator vma_allocator) {
	this->vma_allocator = vma_allocator;
	for (uint32_t i=0; i < this->host_primitives_data_info.size(); i++) {
		if (this->host_primitives_data_info[i].get_texture_size()) {
			uint32_t mipmaps_count = vulkan_helper::get_mipmap_count(this->host_primitives_data_info[i].image_extent);
			VkImageCreateInfo image_create_info = {
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0,VK_IMAGE_TYPE_2D,
					VK_FORMAT_R8G8B8A8_UNORM, this->host_primitives_data_info[i].image_extent,
					mipmaps_count,
					this->host_primitives_data_info[i].image_layers,VK_SAMPLE_COUNT_1_BIT,VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
					VK_SHARING_MODE_EXCLUSIVE,0,nullptr,VK_IMAGE_LAYOUT_UNDEFINED
			};
			VmaAllocationCreateInfo vma_allocation_create_info = {0};
			vma_allocation_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
			check_error(vmaCreateImage(vma_allocator, &image_create_info, &vma_allocation_create_info, &this->device_primitives_data_info[i].image,
					&this->device_primitives_data_info[i].allocation, nullptr),vulkan_helper::Error::IMAGE_CREATION_FAILED);

			VkImageViewCreateInfo image_view_create_info = {
					VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					nullptr,
					0,
					this->device_primitives_data_info[i].image,
					VK_IMAGE_VIEW_TYPE_2D_ARRAY,
					VK_FORMAT_R8G8B8A8_UNORM,
					{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
					{VK_IMAGE_ASPECT_COLOR_BIT, 0, mipmaps_count, 0, this->host_primitives_data_info[i].image_layers}
			};
			check_error(vkCreateImageView(device, &image_view_create_info, nullptr, &this->device_primitives_data_info[i].image_view),
					vulkan_helper::Error::IMAGE_VIEW_CREATION_FAILED);

			VkSamplerCreateInfo sampler_create_info = {
					VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,nullptr,0,
					VK_FILTER_LINEAR,
					VK_FILTER_LINEAR,
					VK_SAMPLER_MIPMAP_MODE_LINEAR,
					VK_SAMPLER_ADDRESS_MODE_REPEAT,
					VK_SAMPLER_ADDRESS_MODE_REPEAT,
					VK_SAMPLER_ADDRESS_MODE_REPEAT,
					mip_bias,
					VK_TRUE,16.0f,
					VK_FALSE,VK_COMPARE_OP_ALWAYS,
					0.0f,static_cast<float>(mipmaps_count),
					VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
					VK_FALSE
			};
			check_error(vkCreateSampler(device, &sampler_create_info, nullptr, &this->device_primitives_data_info[i].sampler), vulkan_helper::Error::SAMPLER_CREATION_FAILED);
		}
	}
}

void VkModel::vk_init_model(VkCommandBuffer cb, VkBuffer host_buffer, uint64_t host_buffer_offset,
                            VkBuffer device_buffer, uint64_t device_buffer_offset) {
    this->vk_record_buffer_copies_from_host_to_device(cb, host_buffer, host_buffer_offset, device_buffer, device_buffer_offset);
    this->vk_init_images(cb, host_buffer, host_buffer_offset);
}

void VkModel::vk_record_buffer_copies_from_host_to_device(VkCommandBuffer cb, VkBuffer host_buffer, uint64_t host_buffer_offset, VkBuffer device_buffer, uint64_t device_buffer_offset) {
	std::vector<VkBufferCopy> buffer_copies;
	for (uint32_t j = 0; j < this->device_primitives_data_info.size(); j++) {
		this->device_primitives_data_info[j].data_buffer = device_buffer;

		VkBufferCopy buffer_copy;
		buffer_copy.srcOffset = host_buffer_offset;
		host_buffer_offset += this->host_primitives_data_info[j].get_total_size();

		device_buffer_offset = vulkan_helper::get_aligned_memory_size(device_buffer_offset, 12);
		buffer_copy.dstOffset = device_buffer_offset;
		this->device_primitives_data_info[j].primitive_vertices_data_offset = device_buffer_offset;
		this->device_primitives_data_info[j].index_data_offset = device_buffer_offset + this->host_primitives_data_info[j].interleaved_vertices_data_size;
		device_buffer_offset += this->host_primitives_data_info[j].get_mesh_and_index_data_size();

		buffer_copy.size = this->host_primitives_data_info[j].get_mesh_and_index_data_size();

		buffer_copies.push_back(buffer_copy);
	}
	vkCmdCopyBuffer(cb, host_buffer, device_buffer, buffer_copies.size(), buffer_copies.data());
}

uint64_t VkModel::vk_init_images(VkCommandBuffer cb, VkBuffer host_image_transient_buffer, uint64_t primitive_host_buffer_offset) {
	std::vector<VkImageMemoryBarrier> image_memory_barriers(this->device_primitives_data_info.size());
	for (uint32_t i = 0; i < image_memory_barriers.size(); i++) {
		image_memory_barriers[i] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			this->device_primitives_data_info[i].image,
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
		};
	}
	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

	uint64_t image_data_offset = primitive_host_buffer_offset;
	for (uint32_t i=0; i < this->device_primitives_data_info.size(); i++) {

		image_data_offset += this->host_primitives_data_info[i].get_mesh_and_index_data_size() + this->host_primitives_data_info[i].image_alignment_size;
		VkBufferImageCopy buffer_image_copy = {
				image_data_offset,
				0,
				0,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, this->host_primitives_data_info[i].image_layers },
				{ 0, 0, 0 },
				this->host_primitives_data_info[i].image_extent
		};
		image_data_offset += this->host_primitives_data_info[i].get_texture_size();
		vkCmdCopyBufferToImage(cb, host_image_transient_buffer, this->device_primitives_data_info[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

		VkImageMemoryBarrier image_memory_barrier;
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_memory_barrier.pNext = nullptr;

		image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.image = this->device_primitives_data_info[i].image;

		image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_memory_barrier.subresourceRange.levelCount = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		VkOffset3D mip_size = { static_cast<int32_t>(this->host_primitives_data_info[i].image_extent.width),
								static_cast<int32_t>(this->host_primitives_data_info[i].image_extent.height),
								static_cast<int32_t>(this->host_primitives_data_info[i].image_extent.depth) };

		// We first transition the j-1 mipmap level to SRC_OPTIMAL, perform the copy to the j mipmap level and transition j-1 to SHADER_READ_ONLY
		for (uint32_t j = 1; j<vulkan_helper::get_mipmap_count(this->host_primitives_data_info[i].image_extent); j++) {
			image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			image_memory_barrier.subresourceRange.baseMipLevel = j-1;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
					nullptr, 1, &image_memory_barrier);

			VkImageBlit image_blit{
					{ VK_IMAGE_ASPECT_COLOR_BIT, j-1, 0, this->host_primitives_data_info[i].image_layers },
					{{ 0, 0, 0 }, mip_size },
					{ VK_IMAGE_ASPECT_COLOR_BIT, j, 0, this->host_primitives_data_info[i].image_layers },
					{{ 0, 0, 0 }, mip_size.x>1 ? mip_size.x/2 : 1, mip_size.y>1 ? mip_size.y/2 : 1, 1 }
			};
			vkCmdBlitImage(cb, this->device_primitives_data_info[i].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					this->device_primitives_data_info[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_blit, VK_FILTER_LINEAR);

			image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);

			if (mip_size.x>1) mip_size.x /= 2;
			if (mip_size.y>1) mip_size.y /= 2;
		}
	}

	// After the blits we need to transition the last mipmap level of all the images (which is still in DST_OPTIMAL) to be shader ready
	for (uint32_t i = 0; i < image_memory_barriers.size(); i++) {
		image_memory_barriers[i] = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				nullptr,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				this->device_primitives_data_info[i].image,
				{VK_IMAGE_ASPECT_COLOR_BIT,vulkan_helper::get_mipmap_count(this->host_primitives_data_info[i].image_extent) - 1,
				 1, 0, VK_REMAINING_ARRAY_LAYERS}
		};
	}
	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, image_memory_barriers.size(), image_memory_barriers.data());

	// Return the offset in the buffer in which defines the end of the data for this model
	return image_data_offset;
}

std::vector<VkWriteDescriptorSet> VkModel::get_descriptor_writes(std::span<VkDescriptorSet> descriptor_sets, VkBuffer uniform_buffer, uint32_t uniform_buffer_offset) {
	std::vector<VkWriteDescriptorSet> writes_descriptor_set(device_primitives_data_info.size()*2); // 2 descriptor write per descriptor

	// Same uniform variables are shared for all primitives
	VkDescriptorBufferInfo *descriptor_buffer_info = new VkDescriptorBufferInfo;
	*descriptor_buffer_info = {
			uniform_buffer,
			uniform_buffer_offset,
			this->copy_uniform_data(nullptr)
	};

	// Each primitive has its own image
	VkDescriptorImageInfo *descriptor_image_infos = new VkDescriptorImageInfo[device_primitives_data_info.size()];

	for (uint32_t i = 0; i < this->device_primitives_data_info.size(); i++) {
		device_primitives_data_info[i].descriptor_set = descriptor_sets[i];

		descriptor_image_infos[i] = {
				this->device_primitives_data_info[i].sampler,
				this->device_primitives_data_info[i].image_view,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		writes_descriptor_set[i*2] = {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				nullptr,
				descriptor_sets[i],
				0,
				0,
				1,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				nullptr,
				descriptor_buffer_info,
				nullptr
		};
		writes_descriptor_set[i*2+1] = {
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				nullptr,
				descriptor_sets[i],
				1,
				0,
				1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				&descriptor_image_infos[i],
				nullptr,
				nullptr
		};
	}
	return writes_descriptor_set;
}

void VkModel::clean_descriptor_writes(std::span<VkWriteDescriptorSet> first_two_write_descriptor_set) {
	delete first_two_write_descriptor_set[0].pBufferInfo;
	delete[] first_two_write_descriptor_set[1].pImageInfo;
}

void VkModel::vk_record_draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t model_set_shader_index) const {
	for (uint32_t i = 0; i < device_primitives_data_info.size(); i++) {
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, model_set_shader_index, 1, &this->device_primitives_data_info[i].descriptor_set, 0, nullptr);
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &device_primitives_data_info[i].data_buffer, &device_primitives_data_info[i].primitive_vertices_data_offset);
		vkCmdBindIndexBuffer(command_buffer, device_primitives_data_info[i].data_buffer, device_primitives_data_info[i].index_data_offset, device_primitives_data_info[i].index_data_type);
		vkCmdDrawIndexed(command_buffer, host_primitives_data_info[i].indices, 1, 0, 0, 0);
	}
}
