#pragma once

#include <string>
#include <cstdint>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "memory/image.h"
#include "renderer/vulkancontext.h"

namespace vpbr {

    class TextureLoader {
    public:
        static bool createSolidColor(const glm::vec4& color, Image& image,
                                     VulkanContext& ctx,
                                     VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

        static bool loadFromFile(const std::string& path, Image& image,
                                 VulkanContext& ctx,
                                 VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    };

} // namespace vpbr