#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

namespace vpbr {

    class VulkanContext; // forward declare

    class Swapchain {
    public:
        void create(VulkanContext& ctx, uint32_t width, uint32_t height);
        void cleanup(VulkanContext& ctx);
        void recreate(VulkanContext& ctx, uint32_t width, uint32_t height);

        VkSwapchainKHR           swapchain = VK_NULL_HANDLE;
        VkFormat                 imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        VkExtent2D               extent = {};
        std::vector<VkImage>     images;
        std::vector<VkImageView> imageViews;

        // Depth buffer
        VkImage         depthImage = VK_NULL_HANDLE;
        VmaAllocation   depthAllocation = VK_NULL_HANDLE;
        VkImageView     depthImageView = VK_NULL_HANDLE;
        VkFormat        depthFormat = VK_FORMAT_D32_SFLOAT;

    private:
        void createSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height);
        void createImageViews(VkDevice device);
        void createDepthResources(VulkanContext& ctx);
        void cleanupSwapchain(VulkanContext& ctx);

        VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats);
        VkPresentModeKHR chooseMode(const std::vector<VkPresentModeKHR>& modes);
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h);
    };

} // namespace vpbr
