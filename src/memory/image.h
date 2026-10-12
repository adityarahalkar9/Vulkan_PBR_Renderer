#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace vpbr {

    class Image {
    public:
        void create(VmaAllocator allocator, uint32_t width, uint32_t height,
                    VkFormat format, VkImageUsageFlags usage,
                    VkImageAspectFlags aspect, uint32_t mipLevels = 1);
        void createView(VkDevice device, VkFormat format, VkImageAspectFlags aspect, uint32_t mipLevels = 1);
        void createSampler(VkDevice device, VkFilter filter = VK_FILTER_LINEAR, uint32_t mipLevels = 1);
        void destroy(VmaAllocator allocator, VkDevice device);

        static void transitionLayout(VkCommandBuffer cmd, VkImage image,
                                     VkImageLayout oldLayout, VkImageLayout newLayout,
                                     uint32_t mipLevels = 1);

        VkImage       image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView   view = VK_NULL_HANDLE;
        VkSampler     sampler = VK_NULL_HANDLE;
        uint32_t      width = 0;
        uint32_t      height = 0;
        uint32_t      mipLevels = 1;
        VkFormat      format = VK_FORMAT_R8G8B8A8_UNORM;
    };

} // namespace vpbr
