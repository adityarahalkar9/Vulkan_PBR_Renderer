#include "memory/image.h"
#include "utils/vulkanutils.h"

#include <stdexcept>

namespace vpbr {

    void Image::create(VmaAllocator allocator, uint32_t width, uint32_t height,
                       VkFormat format, VkImageUsageFlags usage,
                       VkImageAspectFlags /*aspect*/, uint32_t mipLevels)
    {
        this->width = width;
        this->height = height;
        this->format = format;
        this->mipLevels = mipLevels;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr),
                 "Failed to create image via VMA");
    }

    void Image::createView(VkDevice device, VkFormat format, VkImageAspectFlags aspect, uint32_t mipLevels) {
        this->format = format;
        this->mipLevels = mipLevels;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view),
                 "Failed to create image view");
    }

    void Image::createSampler(VkDevice device, VkFilter filter, uint32_t mipLevels) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = filter;
        samplerInfo.minFilter = filter;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(mipLevels);

        VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler),
                 "Failed to create image sampler");
    }

    void Image::destroy(VmaAllocator allocator, VkDevice device) {
        if(sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, sampler, nullptr);
            sampler = VK_NULL_HANDLE;
        }

        if(view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }

        if(image != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, image, allocation);
            image = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }

        width = 0;
        height = 0;
        mipLevels = 1;
    }

    void Image::transitionLayout(VkCommandBuffer cmd, VkImage image,
                                 VkImageLayout oldLayout, VkImageLayout newLayout,
                                 uint32_t mipLevels)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
           newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
           newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        vkCmdPipelineBarrier(
            cmd,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

} // namespace vpbr
