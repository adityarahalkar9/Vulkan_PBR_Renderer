#include "renderer/Swapchain.h"
#include "renderer/VulkanContext.h"
#include "utils/Logger.h"
#include "utils/VulkanUtils.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vpbr {

    void Swapchain::create(VulkanContext& ctx, uint32_t width, uint32_t height) {
        LOG_INFO("Creating swapchain (%ux%u)...", width, height);
        createSwapchain(ctx, width, height);
        createImageViews(ctx.device);
        createDepthResources(ctx);
        LOG_INFO("Swapchain initialization complete.");
    }

    void Swapchain::cleanup(VulkanContext& ctx) {
        cleanupSwapchain(ctx);
    }

    void Swapchain::recreate(VulkanContext& ctx, uint32_t width, uint32_t height) {
        LOG_INFO("Recreating swapchain (%ux%u)...", width, height);
        ctx.waitIdle();
        cleanupSwapchain(ctx);
        create(ctx, width, height);
    }

    void Swapchain::cleanupSwapchain(VulkanContext& ctx) {
        if(depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx.device, depthImageView, nullptr);
            depthImageView = VK_NULL_HANDLE;
        }

        if(depthImage != VK_NULL_HANDLE) {
            vmaDestroyImage(ctx.allocator, depthImage, depthAllocation);
            depthImage = VK_NULL_HANDLE;
            depthAllocation = VK_NULL_HANDLE;
        }

        for(VkImageView view : imageViews) {
            if(view != VK_NULL_HANDLE) {
                vkDestroyImageView(ctx.device, view, nullptr);
            }
        }
        imageViews.clear();
        images.clear();

        if(swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(ctx.device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }

        LOG_INFO("Swapchain cleaned up.");
    }

    void Swapchain::createSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height) {
        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physicalDevice, ctx.surface, &caps),
                 "Failed to get physical device surface capabilities");

        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice, ctx.surface, &formatCount, nullptr),
                 "Failed to get physical device surface formats count");
        if(formatCount == 0) {
            throw std::runtime_error("No surface formats found for surface");
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physicalDevice, ctx.surface, &formatCount, formats.data()),
                 "Failed to get physical device surface formats");

        uint32_t presentModeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice, ctx.surface, &presentModeCount, nullptr),
                 "Failed to get physical device surface present modes count");
        if(presentModeCount == 0) {
            throw std::runtime_error("No surface present modes found for surface");
        }
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physicalDevice, ctx.surface, &presentModeCount, presentModes.data()),
                 "Failed to get physical device surface present modes");

        VkSurfaceFormatKHR surfaceFormat = chooseFormat(formats);
        VkPresentModeKHR presentMode = chooseMode(presentModes);
        extent = chooseExtent(caps, width, height);
        imageFormat = surfaceFormat.format;

        uint32_t imageCount = caps.minImageCount + 1;
        if(caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
            imageCount = caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = ctx.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[] = {ctx.graphicsQueueFamily, ctx.presentQueueFamily};
        if(ctx.graphicsQueueFamily != ctx.presentQueueFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = caps.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSwapchainKHR(ctx.device, &createInfo, nullptr, &swapchain),
                 "Failed to create Vulkan swapchain");

        uint32_t actualImageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(ctx.device, swapchain, &actualImageCount, nullptr),
                 "Failed to get swapchain images count");
        images.resize(actualImageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(ctx.device, swapchain, &actualImageCount, images.data()),
                 "Failed to get swapchain images");

        LOG_INFO("Swapchain created with %u images (Format: %d, Extent: %ux%u)",
                 actualImageCount, static_cast<int>(imageFormat), extent.width, extent.height);
    }

    void Swapchain::createImageViews(VkDevice device) {
        imageViews.resize(images.size());
        for(size_t i = 0; i < images.size(); ++i) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]),
                     "Failed to create swapchain image view");
        }
        LOG_INFO("Created %zu swapchain image views.", imageViews.size());
    }

    void Swapchain::createDepthResources(VulkanContext& ctx) {
        depthFormat = findDepthFormat(ctx.physicalDevice);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(ctx.allocator, &imageInfo, &allocInfo, &depthImage, &depthAllocation, nullptr),
                 "Failed to create swapchain depth image via VMA");

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(ctx.device, &viewInfo, nullptr, &depthImageView),
                 "Failed to create swapchain depth image view");

        LOG_INFO("Swapchain depth resources created (Format: %d).", static_cast<int>(depthFormat));
    }

    VkSurfaceFormatKHR Swapchain::chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        for(const auto& availableFormat : formats) {
            if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
               availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return formats[0];
    }

    VkPresentModeKHR Swapchain::chooseMode(const std::vector<VkPresentModeKHR>& modes) {
        for(const auto& availableMode : modes) {
            if(availableMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availableMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) {
        if(caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return caps.currentExtent;
        }
        VkExtent2D actualExtent = {w, h};
        actualExtent.width = std::clamp(actualExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return actualExtent;
    }

} // namespace vpbr
