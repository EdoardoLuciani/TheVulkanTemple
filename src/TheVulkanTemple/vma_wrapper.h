#ifndef THEVULKANTEMPLE_VMA_WRAPPER_H
#define THEVULKANTEMPLE_VMA_WRAPPER_H

#include "external/volk.h"
#include "external/vk_mem_alloc.h"

class VmaWrapper {
	public:
		VmaWrapper(VkInstance instance, VkPhysicalDevice p_device, VkDevice device, uint32_t vulkan_api_version, VmaAllocatorCreateFlags flags, uint64_t max_shared_allocation_size);
		~VmaWrapper();
		VmaAllocator get_allocator() {return this->allocator;};
	private:
		VmaAllocator allocator;
};

#endif
