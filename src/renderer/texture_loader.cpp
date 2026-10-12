#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "renderer/texture_loader.h"
#include "memory/buffer.h"
#include "utils/logger.h"

#include <cstring>

namespace vpbr {

    static void uploadPixels(VulkanContext& ctx, Image& image,
                             const unsigned char* pixels,
                             uint32_t width, uint32_t height, VkFormat format)
    {
        VkDeviceSize imageSize = width * height * 4;

        Buffer staging;
        staging.create(ctx.allocator, imageSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_MEMORY_USAGE_CPU_ONLY);
        staging.upload(ctx.allocator, pixels, imageSize);

        image.create(ctx.allocator, width, height, format,
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT, 1);
        image.createView(ctx.device, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        image.createSampler(ctx.device, VK_FILTER_LINEAR, 1);

        VkCommandBuffer cmd = ctx.beginSingleTimeCommands();

        Image::transitionLayout(cmd, image.image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(cmd, staging.buffer, image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        Image::transitionLayout(cmd, image.image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

        ctx.endSingleTimeCommands(cmd);
        staging.destroy(ctx.allocator);
    }

    bool TextureLoader::createSolidColor(const glm::vec4& color, Image& image,
                                         VulkanContext& ctx, VkFormat format)
    {
        unsigned char pixel[4];
        pixel[0] = static_cast<unsigned char>(color.r * 255.0f);
        pixel[1] = static_cast<unsigned char>(color.g * 255.0f);
        pixel[2] = static_cast<unsigned char>(color.b * 255.0f);
        pixel[3] = static_cast<unsigned char>(color.a * 255.0f);

        uploadPixels(ctx, image, pixel, 1, 1, format);
        return true;
    }

    bool TextureLoader::loadFromFile(const std::string& path, Image& image,
                                     VulkanContext& ctx, VkFormat format)
    {
        int width, height, channels;
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if(!pixels) {
            LOG_ERROR("Failed to load: %s", path.c_str());
            return false;
        }

        uploadPixels(ctx, image, pixels, width, height, format);
        stbi_image_free(pixels);
        return true;
    }

} // namespace vpbr