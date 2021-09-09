#ifndef THEVULKANTEMPLE_VK_BUFFERS_SUBALLOCATOR_H
#define THEVULKANTEMPLE_VK_BUFFERS_SUBALLOCATOR_H

#include <vector>
#include <array>
#include <map>
#include <cstdint>
#include <utility>
#include <unordered_map>
#include <cmath>
#include "external/volk.h"
#include "external/vk_mem_alloc.h"

class VkBuffersBuddySubAllocator {
	public:
		VkBuffersBuddySubAllocator(VmaAllocator vma_allocator, VkBufferUsageFlags buffer_usage_flags, VmaMemoryUsage vma_memory_usage,
				uint64_t block_initial_size, uint64_t min_allocation_size = 32);
		~VkBuffersBuddySubAllocator();

		void vk_record_buffers_pipeline_barrier(VkCommandBuffer cb, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
				uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

		struct sub_allocation_data {
			VkBuffer buffer;
			uint64_t buffer_offset;
			void *allocation_host_ptr;
		};
		sub_allocation_data suballocate(uint64_t size, uint64_t alignment = 1);
		void free(const sub_allocation_data& to_free);
	private:
		VmaAllocator vma_allocator;
		VkBufferUsageFlags buffer_usage_flags;
		VmaMemoryUsage vma_memory_usage;
		uint64_t block_initial_size;
		uint64_t min_allocation_size;

		struct buffer_unit_data {
			VmaAllocation allocation;
			void *host_ptr;

			// red-black binary tree to keep size|address
			std::multimap<uint64_t, uint64_t> free_blocks;

			struct used_block {
				uint64_t size;
				uint64_t po2_alignment_increment;
			};
			// hashmap to keep address|used_block
			std::unordered_map<uint64_t, used_block> used_blocks;
		};
		std::unordered_map<VkBuffer, buffer_unit_data> buffer_units;

		decltype(VkBuffersBuddySubAllocator::buffer_units)::iterator request_next_buffer(uint64_t buffer_size = 0);

		std::pair<uint64_t, uint64_t> split_block_recursive(std::multimap<uint64_t, uint64_t>& buffer_free_blocks,
				std::multimap<uint64_t, uint64_t>::iterator block_insert_hint, uint64_t new_block_sizes,
				uint64_t old_block_address, uint64_t requested_block_size);
		void merge_blocks_recursive(std::multimap<uint64_t, uint64_t>& buffer_free_blocks, std::pair<uint64_t, uint64_t> address_size_source_block);
};

#endif
