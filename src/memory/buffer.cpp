#include "memory/buffer.h"
#include "utils/vulkanutils.h"

#include <cstring>

namespace vpbr {

    void Buffer::create(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage) {
        this->size = size;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memUsage;

        VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr),
                 "Failed to create buffer via VMA");
    }

    void Buffer::destroy(VmaAllocator allocator) {
        if(buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer, allocation);
            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
            size = 0;
        }
    }

    void Buffer::upload(VmaAllocator allocator, const void* data, VkDeviceSize uploadSize) {
        void* mapped = nullptr;
        VK_CHECK(vmaMapMemory(allocator, allocation, &mapped), "Failed to map buffer memory for upload");
        std::memcpy(mapped, data, static_cast<size_t>(uploadSize));
        vmaUnmapMemory(allocator, allocation);
    }

    void* Buffer::map(VmaAllocator allocator) {
        void* mapped = nullptr;
        VK_CHECK(vmaMapMemory(allocator, allocation, &mapped), "Failed to map buffer memory");
        return mapped;
    }

    void Buffer::unmap(VmaAllocator allocator) {
        vmaUnmapMemory(allocator, allocation);
    }

} // namespace vpbr
