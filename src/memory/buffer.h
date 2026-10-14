#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace vpbr{
    class Buffer{
        public:
            void create(VmaAllocator allocator, VkDeviceSize size,VkBufferUsageFlags usage, VmaMemoryUsage memUsage);
            void destroy(VmaAllocator allocator);
            void upload(VmaAllocator allocator, const void* data, VkDeviceSize size);
            void* map(VmaAllocator allocator);
            void unmap(VmaAllocator allocator);

            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            VkDeviceSize size = 0;
    };
} 