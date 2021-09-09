#include "vma_wrapper.h"

VmaWrapper::VmaWrapper(VkInstance instance, VkPhysicalDevice p_device, VkDevice device, uint32_t vulkan_api_version, VmaAllocatorCreateFlags flags, uint64_t max_shared_allocation_size) {
	VmaAllocatorCreateInfo vma_allocator_create_info = {0};
	vma_allocator_create_info.flags = flags;
	vma_allocator_create_info.physicalDevice = p_device;
	vma_allocator_create_info.device = device;
	vma_allocator_create_info.preferredLargeHeapBlockSize = max_shared_allocation_size; // blocks larger than 512 MB get allocated in a separate VkDeviceMemory

	VmaVulkanFunctions vma_vulkan_functions = {
			vkGetPhysicalDeviceProperties, vkGetPhysicalDeviceMemoryProperties, vkAllocateMemory, vkFreeMemory,
			vkMapMemory, vkUnmapMemory, vkFlushMappedMemoryRanges, vkInvalidateMappedMemoryRanges,
			vkBindBufferMemory, vkBindImageMemory, vkGetBufferMemoryRequirements, vkGetImageMemoryRequirements,
			vkCreateBuffer, vkDestroyBuffer, vkCreateImage, vkDestroyImage, vkCmdCopyBuffer,
			vkGetBufferMemoryRequirements2, vkGetImageMemoryRequirements2, vkBindBufferMemory2,vkBindImageMemory2, vkGetPhysicalDeviceMemoryProperties2
	};
	vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_functions;
	vma_allocator_create_info.instance = instance;
	vma_allocator_create_info.vulkanApiVersion = VK_MAKE_VERSION(1, 1, 0);
	vmaCreateAllocator(&vma_allocator_create_info, &allocator);
}

VmaWrapper::~VmaWrapper() {
	vmaDestroyAllocator(this->allocator);
}
