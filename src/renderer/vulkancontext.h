#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL.h>
#include <vector>
#include <string>

namespace vpbr {

    class VulkanContext {
    public:
        void init(SDL_Window* window);
        void cleanup();
        void waitIdle();

        // Vulkan core objects
        VkInstance               instance = VK_NULL_HANDLE;
        VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;
        VkDevice                 device = VK_NULL_HANDLE;
        VkSurfaceKHR             surface = VK_NULL_HANDLE;

        // Queue families and queues
        uint32_t graphicsQueueFamily = 0;
        uint32_t presentQueueFamily = 0;
        uint32_t transferQueueFamily = 0;
        VkQueue  graphicsQueue = VK_NULL_HANDLE;
        VkQueue  presentQueue = VK_NULL_HANDLE;
        VkQueue  transferQueue = VK_NULL_HANDLE;

        // Command pools
        VkCommandPool graphicsCommandPool = VK_NULL_HANDLE;
        VkCommandPool transferCommandPool = VK_NULL_HANDLE;

        // VMA allocator
        VmaAllocator allocator = VK_NULL_HANDLE;

        // Device properties
        VkPhysicalDeviceProperties      deviceProperties{};
        VkPhysicalDeviceMemoryProperties memoryProperties{};

        // Debug
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

        // Helpers
        VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool = VK_NULL_HANDLE);
        void endSingleTimeCommands(VkCommandBuffer cmd, VkCommandPool pool = VK_NULL_HANDLE, VkQueue queue = VK_NULL_HANDLE);

    private:
        void createInstance();
        void setupDebugMessenger();
        void createSurface(SDL_Window* window);
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPools();
        void createAllocator();

        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();

        bool enableValidationLayers = true;
        std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    };

} // namespace vpbr
