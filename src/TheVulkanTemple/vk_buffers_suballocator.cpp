#include "vk_buffers_suballocator.h"
#include "vulkan_helper.h"
#include <cmath>
#include <bit>

VkBuffersBuddySubAllocator::VkBuffersBuddySubAllocator(VmaAllocator vma_allocator, VkBufferUsageFlags buffer_usage_flags,
		VmaMemoryUsage vma_memory_usage, uint64_t block_initial_size, uint64_t min_allocation_size) :
vma_allocator{vma_allocator}, buffer_usage_flags{buffer_usage_flags}, vma_memory_usage{vma_memory_usage},
	block_initial_size{std::bit_ceil(block_initial_size)}, min_allocation_size{std::bit_ceil(min_allocation_size)} {
	request_next_buffer();
}

VkBuffersBuddySubAllocator::~VkBuffersBuddySubAllocator() {
	for (auto& bu : buffer_units) {
		vmaDestroyBuffer(vma_allocator, bu.first, bu.second.allocation);
	}
}

void VkBuffersBuddySubAllocator::vk_record_buffers_pipeline_barrier(VkCommandBuffer cb, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
		uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
	std::vector<VkBufferMemoryBarrier> memory_barriers;
	memory_barriers.reserve(this->buffer_units.size());
	for (const auto& bu : buffer_units) {
		memory_barriers.push_back({
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				nullptr,
				srcAccessMask,
				dstAccessMask,
				srcQueueFamilyIndex,
				dstQueueFamilyIndex,
				bu.first,
				0,
				VK_WHOLE_SIZE
		});
	}
	vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, 0, 0, nullptr, memory_barriers.size(), memory_barriers.data(), 0, nullptr);
}

VkBuffersBuddySubAllocator::sub_allocation_data VkBuffersBuddySubAllocator::suballocate(uint64_t size, uint64_t alignment) {
	// The alignment is split in two parts:
	// - the first is a power of 2 which it is used to look for suitable blocks,
	// - the second one is a predicted residual part that becomes part of the allocation size, note however that the real
	// residual is calculated at the moment of block allocation, this prediction is the worst case scenario and guarantees
	// that the block has enough size to accomodate data + alignment
	uint64_t floored_alignment = std::bit_floor(alignment);
	uint64_t predicted_alignment_increment = alignment - floored_alignment;
	size = std::bit_ceil(std::max(size + predicted_alignment_increment, min_allocation_size));

	auto get_suballocation_return_value = [](decltype(buffer_units)::iterator bu, std::pair<uint64_t, uint64_t> selected_block, uint64_t alignment) -> sub_allocation_data {
		// Here the correct increment is calculated
		uint64_t corrected_alignment_increment = alignment * std::ceil(selected_block.second/static_cast<float>(alignment)) - selected_block.second;
		bu->second.used_blocks.emplace(selected_block.second + corrected_alignment_increment,
				buffer_unit_data::used_block{ selected_block.first, corrected_alignment_increment });

		void *allocation_ptr = nullptr;
		if (bu->second.host_ptr != nullptr) {
			allocation_ptr = static_cast<uint8_t*>(bu->second.host_ptr) + selected_block.second + corrected_alignment_increment;
		}
		return { bu->first, selected_block.second + corrected_alignment_increment, allocation_ptr};
	};

	for (auto bu = buffer_units.begin(); bu!=buffer_units.end();) {
		auto first_block = bu->second.free_blocks.upper_bound(size-1);
		std::pair<uint64_t, uint64_t> new_block_info;

		for (auto& block_it = first_block; block_it!=bu->second.free_blocks.end(); block_it++) {
			// If the block has the same size as requested and the correct alignment then it is selected right away
			if (block_it->first==size && block_it->second%floored_alignment==0) {
				new_block_info = *block_it;
				bu->second.free_blocks.erase(block_it);
				return get_suballocation_return_value(bu, new_block_info, alignment);
			}
			// If the block has correct alignment but not the right size it is split up and then selected
			else if (block_it->second%floored_alignment==0) {
				uint64_t old_block_half_size = first_block->first/2;
				uint64_t old_address = first_block->second;
				new_block_info = split_block_recursive(bu->second.free_blocks, bu->second.free_blocks.erase(first_block),
						old_block_half_size, old_address, size);
				return get_suballocation_return_value(bu, new_block_info, alignment);
			}
		}

		std::next(bu) == buffer_units.end() ? bu = this->request_next_buffer(size) : bu++;
	}
	// suballocation is not possible
	return { VK_NULL_HANDLE, 0 };
}

void VkBuffersBuddySubAllocator::free(const sub_allocation_data& to_free) {
	auto block_to_free_buffer_unit = buffer_units.find(to_free.buffer);
	buffer_unit_data &block_to_free_buffer_data = block_to_free_buffer_unit->second;
	auto block_to_be_freed = block_to_free_buffer_data.used_blocks.find(to_free.buffer_offset);
	auto block_to_merge = std::pair<uint64_t, uint64_t>{block_to_be_freed->first - block_to_be_freed->second.po2_alignment_increment, block_to_be_freed->second.size};
	block_to_free_buffer_data.used_blocks.erase(block_to_be_freed);
	merge_blocks_recursive(block_to_free_buffer_data.free_blocks, block_to_merge);

	if (block_to_free_buffer_data.used_blocks.empty()) {
        if (vma_memory_usage == VMA_MEMORY_USAGE_CPU_ONLY || vma_memory_usage == VMA_MEMORY_USAGE_CPU_TO_GPU) {
            vmaUnmapMemory(vma_allocator, block_to_free_buffer_unit->second.allocation);
        }
		vmaDestroyBuffer(vma_allocator, block_to_free_buffer_unit->first, block_to_free_buffer_unit->second.allocation);
		buffer_units.erase(block_to_free_buffer_unit);
	}
}

decltype(VkBuffersBuddySubAllocator::buffer_units)::iterator VkBuffersBuddySubAllocator::request_next_buffer(uint64_t buffer_size) {
	// Round the size to the min power of 2, else that space is wasted
	buffer_size = std::bit_ceil(std::max(buffer_size, block_initial_size));

	VkBufferCreateInfo buffer_create_info = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			nullptr,
			0,
			buffer_size,
			buffer_usage_flags,
			VK_SHARING_MODE_EXCLUSIVE,
			0, nullptr
	};
	VmaAllocationCreateInfo allocation_create_info = {};
	allocation_create_info.usage = vma_memory_usage;

	VkBuffer buffer;
	buffer_unit_data buffer_unit_to_append;
	vulkan_helper::check_error(
			vmaCreateBuffer(vma_allocator, &buffer_create_info, &allocation_create_info, &buffer, &buffer_unit_to_append.allocation,nullptr),
			vulkan_helper::Error::BUFFER_CREATION_FAILED);
	buffer_unit_to_append.free_blocks.insert(std::pair<uint64_t, uint64_t>{buffer_size, 0});

    buffer_unit_to_append.host_ptr = nullptr;
	if (vma_memory_usage == VMA_MEMORY_USAGE_CPU_ONLY || vma_memory_usage == VMA_MEMORY_USAGE_CPU_TO_GPU) {
		vulkan_helper::check_error(vmaMapMemory(vma_allocator, buffer_unit_to_append.allocation, &buffer_unit_to_append.host_ptr), vulkan_helper::Error::MEMORY_MAP_FAILED);
	}

	return buffer_units.emplace(std::make_pair(buffer, buffer_unit_to_append)).first;
}

std::pair<uint64_t, uint64_t> VkBuffersBuddySubAllocator::split_block_recursive(std::multimap<uint64_t, uint64_t>& buffer_free_blocks,
		std::multimap<uint64_t, uint64_t>::iterator block_insert_hint, uint64_t new_block_sizes, uint64_t old_block_address, uint64_t requested_block_size) {
	// creating the right block
	block_insert_hint = buffer_free_blocks.emplace_hint(block_insert_hint, new_block_sizes, old_block_address + new_block_sizes);
	if (new_block_sizes != requested_block_size) {
		// continuing to subdivide the left block without actually creating it
		return split_block_recursive(buffer_free_blocks, block_insert_hint, new_block_sizes/2, old_block_address, requested_block_size);
	}
	else {
		// on the last step we return the data of the left block, but we do not create it, since it is going to be removed shortly after
		return {new_block_sizes, old_block_address};
	}
}

void VkBuffersBuddySubAllocator::merge_blocks_recursive(std::multimap<uint64_t, uint64_t>& buffer_free_blocks, std::pair<uint64_t, uint64_t> address_size_source_block) {
	int64_t left_adjacent_block_address = address_size_source_block.first - address_size_source_block.second;
	int64_t right_adjacent_block_address = address_size_source_block.first + address_size_source_block.second;
	auto its = buffer_free_blocks.equal_range(address_size_source_block.second);
	for (auto it = its.first; it != its.second; it++) {
		if (it->second == left_adjacent_block_address) {
			buffer_free_blocks.erase(it);
			// The new block has the left adjacent block as the address
			return merge_blocks_recursive(buffer_free_blocks, {left_adjacent_block_address, address_size_source_block.second*2});
		}
		else if (it->second == right_adjacent_block_address) {
			buffer_free_blocks.erase(it);
			// The new block has the source block address as the address
			return merge_blocks_recursive(buffer_free_blocks, {address_size_source_block.first, address_size_source_block.second*2});
		}
	}
	// When there are no other blocks which can be joined, insert the merged block into the free ones
	buffer_free_blocks.emplace(address_size_source_block.second, address_size_source_block.first);
	return;
}