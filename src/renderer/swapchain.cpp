#include "renderer/swapchain.h"
#include "renderer/vulkancontext.h"
#include "utils/logger.h"
#include "utils/vulkanutils.h"

#include <algorithm>
#include <limits>

namespace vpbr {

    void Swapchain::create(VulkanContext& ctx, uint32_t width, uint32_t height) {
        LOG_INFO("Creating swapchain %ux%u...", width, height);
        createSwapchain(ctx, width, height);
        createImageViews(ctx.device);
        createDepthResources(ctx);
    }

    void Swapchain::cleanup(VulkanContext& ctx) {
        cleanupSwapchain(ctx);
    }

    void Swapchain::recreate(VulkanContext& ctx, uint32_t width, uint32_t height) {
        LOG_INFO("Recreating swapchain %ux%u...", width, height);
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
        for(auto view : imageViews) {
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
    }

    void Swapchain::createSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height) {
        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            ctx.physicalDevice, ctx.surface, &caps), "");

        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            ctx.physicalDevice, ctx.surface, &formatCount, nullptr), "");
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            ctx.physicalDevice, ctx.surface, &formatCount, formats.data()), "");

        uint32_t modeCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            ctx.physicalDevice, ctx.surface, &modeCount, nullptr), "");
        std::vector<VkPresentModeKHR> modes(modeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            ctx.physicalDevice, ctx.surface, &modeCount, modes.data()), "");

        VkSurfaceFormatKHR format = chooseFormat(formats);
        VkPresentModeKHR mode = chooseMode(modes);
        extent = chooseExtent(caps, width, height);
        imageFormat = format.format;

        uint32_t imageCount = caps.minImageCount + 1;
        if(caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
            imageCount = caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface = ctx.surface;
        ci.minImageCount = imageCount;
        ci.imageFormat = format.format;
        ci.imageColorSpace = format.colorSpace;
        ci.imageExtent = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t families[] = {ctx.graphicsQueueFamily, ctx.presentQueueFamily};
        if(ctx.graphicsQueueFamily != ctx.presentQueueFamily) {
            ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices = families;
        }
        else {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = mode;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSwapchainKHR(ctx.device, &ci, nullptr, &swapchain), "");

        uint32_t actualCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(ctx.device, swapchain, &actualCount, nullptr), "");
        images.resize(actualCount);
        VK_CHECK(vkGetSwapchainImagesKHR(ctx.device, swapchain, &actualCount, images.data()), "");

        LOG_INFO("Swapchain: %u images, extent %ux%u",
                 actualCount, extent.width, extent.height);
    }

    void Swapchain::createImageViews(VkDevice device) {
        imageViews.resize(images.size());

        for(size_t i = 0; i < images.size(); i++) {
            VkImageViewCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = images[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = imageFormat;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(device, &info, nullptr, &imageViews[i]), "");
        }
    }

    void Swapchain::createDepthResources(VulkanContext& ctx) {
        depthFormat = findDepthFormat(ctx.physicalDevice);

        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent.width = extent.width;
        info.extent.height = extent.height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.format = depthFormat;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(ctx.allocator, &info, &allocInfo,
                                &depthImage, &depthAllocation, nullptr), "");

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(ctx.device, &viewInfo, nullptr, &depthImageView), "");
    }

    VkSurfaceFormatKHR Swapchain::chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        for(const auto& f : formats) {
            if(f.format == VK_FORMAT_B8G8R8A8_SRGB &&
               f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return f;
            }
        }
        return formats[0];
    }

    VkPresentModeKHR Swapchain::chooseMode(const std::vector<VkPresentModeKHR>& modes) {
        for(const auto& m : modes) {
            if(m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps,
                                       uint32_t w, uint32_t h)
    {
        if(caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return caps.currentExtent;
        }
        VkExtent2D actual = {w, h};
        actual.width = std::clamp(actual.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        actual.height = std::clamp(actual.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return actual;
    }

} // namespace vpbr