#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "renderer/vulkancontext.h"
#include "utils/logger.h"
#include "utils/vulkanutils.h"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace vpbr {

    namespace {

        VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* /*pUserData*/)
        {
            if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                LOG_ERROR("Vulkan Validation: %s", pCallbackData->pMessage);
            }
            else if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                LOG_WARN("Vulkan Validation: %s", pCallbackData->pMessage);
            }
            else {
                LOG_DEBUG("Vulkan Validation: %s", pCallbackData->pMessage);
            }
            return VK_FALSE;
        }

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = debugCallback;
        }

    } // namespace

    void VulkanContext::init(SDL_Window* window) {
        LOG_INFO("Initializing VulkanContext...");
        createInstance();
        setupDebugMessenger();
        createSurface(window);
        pickPhysicalDevice();
        createLogicalDevice();
        createAllocator();
        createCommandPools();
        LOG_INFO("VulkanContext initialized successfully.");
    }

    void VulkanContext::cleanup() {
        waitIdle();

        if(transferCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, transferCommandPool, nullptr);
            transferCommandPool = VK_NULL_HANDLE;
        }
        if(graphicsCommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
            graphicsCommandPool = VK_NULL_HANDLE;
        }
        if(allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
        if(device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if(surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if(debugMessenger != VK_NULL_HANDLE) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if(func != nullptr) {
                func(instance, debugMessenger, nullptr);
            }
            debugMessenger = VK_NULL_HANDLE;
        }
        if(instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        LOG_INFO("VulkanContext cleaned up.");
    }

    void VulkanContext::waitIdle() {
        if(device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    void VulkanContext::createInstance() {
        if(enableValidationLayers && !checkValidationLayerSupport()) {
            LOG_WARN("Validation layers requested, but not available. Disabling validation layers.");
            enableValidationLayers = false;
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VulkanPBR";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "vpbr";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "Failed to create Vulkan instance");
        LOG_INFO("Vulkan instance created.");
    }

    void VulkanContext::setupDebugMessenger() {
        if(!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if(func != nullptr) {
            VK_CHECK(func(instance, &createInfo, nullptr, &debugMessenger), "Failed to set up debug messenger");
            LOG_INFO("Vulkan debug messenger initialized.");
        }
        else {
            LOG_WARN("vkCreateDebugUtilsMessengerEXT procedure address not found.");
        }
    }

    void VulkanContext::createSurface(SDL_Window* window) {
        if(!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            throw std::runtime_error(std::string("Failed to create window surface: ") + SDL_GetError());
        }
        LOG_INFO("Vulkan surface created.");
    }

    void VulkanContext::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "Failed to enumerate physical devices count");
        if(deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "Failed to enumerate physical devices");

        VkPhysicalDevice chosen = VK_NULL_HANDLE;
        int highestScore = -1;

        for(const auto& dev : devices) {
            VkPhysicalDeviceProperties props;
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceProperties(dev, &props);
            vkGetPhysicalDeviceFeatures(dev, &features);

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

            bool hasGraphics = false;
            bool hasPresent = false;
            for(uint32_t i = 0; i < queueFamilyCount; ++i) {
                if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    hasGraphics = true;
                }
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
                if(presentSupport) {
                    hasPresent = true;
                }
            }

            if(!hasGraphics || !hasPresent) continue;

            uint32_t extCount = 0;
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> availableExts(extCount);
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availableExts.data());
            bool hasSwapchain = false;
            for(const auto& ext : availableExts) {
                if(std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    hasSwapchain = true;
                    break;
                }
            }
            if(!hasSwapchain) continue;

            int score = 10;
            if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 1000;
            }
            else if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                score += 100;
            }

            if(score > highestScore) {
                highestScore = score;
                chosen = dev;
            }
        }

        if(chosen == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU with required queue families and swapchain extension");
        }

        physicalDevice = chosen;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        LOG_INFO("Selected physical device: %s", deviceProperties.deviceName);
    }

    void VulkanContext::createLogicalDevice() {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        int gFamily = -1;
        int pFamily = -1;
        int tFamily = -1;

        for(uint32_t i = 0; i < queueFamilyCount; ++i) {
            if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if(gFamily == -1) gFamily = static_cast<int>(i);
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if(presentSupport) {
                if(pFamily == -1 || (gFamily != -1 && i == static_cast<uint32_t>(gFamily))) {
                    pFamily = static_cast<int>(i);
                }
            }

            if((queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
               !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                if(tFamily == -1) tFamily = static_cast<int>(i);
            }
        }

        if(tFamily == -1) {
            tFamily = gFamily;
        }

        if(gFamily == -1 || pFamily == -1) {
            throw std::runtime_error("Failed to find required queue families for logical device");
        }

        graphicsQueueFamily = static_cast<uint32_t>(gFamily);
        presentQueueFamily = static_cast<uint32_t>(pFamily);
        transferQueueFamily = static_cast<uint32_t>(tFamily);

        std::set<uint32_t> uniqueQueueFamilies = {
            graphicsQueueFamily,
            presentQueueFamily,
            transferQueueFamily
        };

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        for(uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "Failed to create logical device");

        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
        vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);

        LOG_INFO("Logical device created (Graphics Q: %u, Present Q: %u, Transfer Q: %u).",
                 graphicsQueueFamily, presentQueueFamily, transferQueueFamily);
    }

    void VulkanContext::createCommandPools() {
        VkCommandPoolCreateInfo graphicsPoolInfo{};
        graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        graphicsPoolInfo.queueFamilyIndex = graphicsQueueFamily;

        VK_CHECK(vkCreateCommandPool(device, &graphicsPoolInfo, nullptr, &graphicsCommandPool),
                 "Failed to create graphics command pool");

        VkCommandPoolCreateInfo transferPoolInfo{};
        transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        transferPoolInfo.queueFamilyIndex = transferQueueFamily;

        VK_CHECK(vkCreateCommandPool(device, &transferPoolInfo, nullptr, &transferCommandPool),
                 "Failed to create transfer command pool");

        LOG_INFO("Command pools created.");
    }

    void VulkanContext::createAllocator() {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = device;
        allocatorInfo.instance = instance;

        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator), "Failed to create VMA allocator");
        LOG_INFO("VMA allocator created.");
    }

    VkCommandBuffer VulkanContext::beginSingleTimeCommands(VkCommandPool pool) {
        if(pool == VK_NULL_HANDLE) {
            pool = graphicsCommandPool;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer),
                 "Failed to allocate single-time command buffer");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo),
                 "Failed to begin single-time command buffer");

        return commandBuffer;
    }

    void VulkanContext::endSingleTimeCommands(VkCommandBuffer cmd, VkCommandPool pool, VkQueue queue) {
        if(pool == VK_NULL_HANDLE) {
            pool = graphicsCommandPool;
        }
        if(queue == VK_NULL_HANDLE) {
            queue = graphicsQueue;
        }

        VK_CHECK(vkEndCommandBuffer(cmd), "Failed to record single-time command buffer");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
                 "Failed to submit single-time command buffer");
        VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue idle after single-time commands");

        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    bool VulkanContext::checkValidationLayerSupport() {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for(const char* layerName : validationLayers) {
            bool layerFound = false;
            for(const auto& layerProperties : availableLayers) {
                if(std::strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if(!layerFound) {
                return false;
            }
        }
        return true;
    }

    std::vector<const char*> VulkanContext::getRequiredExtensions() {
        Uint32 sdlExtensionCount = 0;
        char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if(!sdlExtensions && sdlExtensionCount > 0) {
            throw std::runtime_error(std::string("Failed to get SDL Vulkan instance extensions: ") + SDL_GetError());
        }

        std::vector<const char*> extensions;
        for(Uint32 i = 0; i < sdlExtensionCount; ++i) {
            extensions.push_back(sdlExtensions[i]);
        }

        if(enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

} // namespace vpbr
