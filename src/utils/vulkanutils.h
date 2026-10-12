#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>

namespace vpbr {

#define VK_CHECK(result, msg) \
    do { \
        VkResult _res = (result); \
        if (_res != VK_SUCCESS) { \
            throw std::runtime_error( \
                std::string(msg) + " (VkResult: " + std::to_string(static_cast<int>(_res)) + ")"); \
        } \
    } while (0)

    inline std::vector<char> readShaderFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if(!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + filename);
        }
        auto fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
        return buffer;
    }

    inline VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule module;
        VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module),
                 "Failed to create shader module");
        return module;
    }

    inline VkFormat findSupportedFormat(
        VkPhysicalDevice physicalDevice,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features)
    {
        for(VkFormat fmt : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, fmt, &props);
            if(tiling == VK_IMAGE_TILING_LINEAR &&
               (props.linearTilingFeatures & features) == features)
                return fmt;
            if(tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features)
                return fmt;
        }
        throw std::runtime_error("Failed to find supported format");
    }

    inline VkFormat findDepthFormat(VkPhysicalDevice physicalDevice) {
        return findSupportedFormat(
            physicalDevice,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    inline bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

} // namespace vpbr
