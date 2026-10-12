#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "renderer/vulkancontext.h"
#include "utils/logger.h"
#include "utils/vulkanutils.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace vpbr {

    namespace {

        VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void*)
        {
            if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                LOG_ERROR("Vulkan: %s", pCallbackData->pMessage);
            }
            else if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                LOG_WARN("Vulkan: %s", pCallbackData->pMessage);
            }
            return VK_FALSE;
        }

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& ci) {
            ci = {};
            ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            ci.pfnUserCallback = debugCallback;
        }

    } // namespace

    void VulkanContext::init(SDL_Window* window) {
        LOG_INFO("Initializing Vulkan...");
        createInstance();
        setupDebugMessenger();
        createSurface(window);
        pickPhysicalDevice();
        createLogicalDevice();
        createAllocator();
        createCommandPools();
        LOG_INFO("Vulkan initialized.");
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
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if(func) func(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if(instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        LOG_INFO("Vulkan cleaned up.");
    }

    void VulkanContext::waitIdle() {
        if(device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    void VulkanContext::createInstance() {
        if(enableValidationLayers && !checkValidationLayerSupport()) {
            LOG_WARN("Validation layers not available, disabling.");
            enableValidationLayers = false;
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VulkanPBR";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "vpbr";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCI{};
        if(enableValidationLayers) {
            ci.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            ci.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCI);
            ci.pNext = &debugCI;
        }

        VK_CHECK(vkCreateInstance(&ci, nullptr, &instance), "Failed to create instance");
        LOG_INFO("Vulkan instance created.");
    }

    void VulkanContext::setupDebugMessenger() {
        if(!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT ci{};
        populateDebugMessengerCreateInfo(ci);

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if(func) {
            VK_CHECK(func(instance, &ci, nullptr, &debugMessenger), "Failed to setup debug messenger");
            LOG_INFO("Debug messenger initialized.");
        }
    }

    void VulkanContext::createSurface(SDL_Window* window) {
        if(!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            throw std::runtime_error(std::string("Failed to create surface: ") + SDL_GetError());
        }
        LOG_INFO("Surface created.");
    }

    void VulkanContext::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "");
        if(deviceCount == 0) throw std::runtime_error("No Vulkan GPUs found");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "");

        VkPhysicalDevice chosen = VK_NULL_HANDLE;
        int bestScore = -1;

        for(const auto& dev : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);

            uint32_t qFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> qFamilies(qFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qFamilyCount, qFamilies.data());

            bool hasGraphics = false, hasPresent = false;
            for(uint32_t i = 0; i < qFamilyCount; i++) {
                if(qFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasGraphics = true;
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
                if(present) hasPresent = true;
            }
            if(!hasGraphics || !hasPresent) continue;

            uint32_t extCount = 0;
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> exts(extCount);
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

            bool hasSwapchain = false;
            for(const auto& ext : exts) {
                if(strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    hasSwapchain = true;
                    break;
                }
            }
            if(!hasSwapchain) continue;

            int score = 10;
            if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
            else if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;

            if(score > bestScore) {
                bestScore = score;
                chosen = dev;
            }
        }

        if(chosen == VK_NULL_HANDLE) {
            throw std::runtime_error("No suitable GPU found");
        }

        physicalDevice = chosen;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        LOG_INFO("GPU: %s", deviceProperties.deviceName);
    }

    void VulkanContext::createLogicalDevice() {
        uint32_t qFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> qFamilies(qFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qFamilyCount, qFamilies.data());

        int gFamily = -1, pFamily = -1, tFamily = -1;

        for(uint32_t i = 0; i < qFamilyCount; i++) {
            if(qFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && gFamily == -1) {
                gFamily = static_cast<int>(i);
            }
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &present);
            if(present && pFamily == -1) {
                pFamily = static_cast<int>(i);
            }
            if((qFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
               !(qFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && tFamily == -1) {
                tFamily = static_cast<int>(i);
            }
        }

        if(tFamily == -1) tFamily = gFamily;
        if(gFamily == -1 || pFamily == -1) {
            throw std::runtime_error("Failed to find queue families");
        }

        graphicsQueueFamily = static_cast<uint32_t>(gFamily);
        presentQueueFamily = static_cast<uint32_t>(pFamily);
        transferQueueFamily = static_cast<uint32_t>(tFamily);

        std::set<uint32_t> uniqueFamilies = {graphicsQueueFamily, presentQueueFamily, transferQueueFamily};

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        float priority = 1.0f;
        for(uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo qi{};
            qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = family;
            qi.queueCount = 1;
            qi.pQueuePriorities = &priority;
            queueInfos.push_back(qi);
        }

        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = VK_TRUE;
        features.fillModeNonSolid = VK_TRUE;

        std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        ci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        ci.pQueueCreateInfos = queueInfos.data();
        ci.pEnabledFeatures = &features;
        ci.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        ci.ppEnabledExtensionNames = deviceExtensions.data();

        VK_CHECK(vkCreateDevice(physicalDevice, &ci, nullptr, &device), "Failed to create device");

        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
        vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);

        LOG_INFO("Device created (GQ:%u PQ:%u TQ:%u)",
                 graphicsQueueFamily, presentQueueFamily, transferQueueFamily);
    }

    void VulkanContext::createCommandPools() {
        VkCommandPoolCreateInfo gPool{};
        gPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        gPool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        gPool.queueFamilyIndex = graphicsQueueFamily;

        VK_CHECK(vkCreateCommandPool(device, &gPool, nullptr, &graphicsCommandPool), "");

        VkCommandPoolCreateInfo tPool{};
        tPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        tPool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        tPool.queueFamilyIndex = transferQueueFamily;

        VK_CHECK(vkCreateCommandPool(device, &tPool, nullptr, &transferCommandPool), "");
        LOG_INFO("Command pools created.");
    }

    void VulkanContext::createAllocator() {
        VmaAllocatorCreateInfo info{};
        info.vulkanApiVersion = VK_API_VERSION_1_3;
        info.physicalDevice = physicalDevice;
        info.device = device;
        info.instance = instance;

        VK_CHECK(vmaCreateAllocator(&info, &allocator), "Failed to create VMA allocator");
        LOG_INFO("VMA allocator created.");
    }

    VkCommandBuffer VulkanContext::beginSingleTimeCommands(VkCommandPool pool) {
        if(pool == VK_NULL_HANDLE) pool = graphicsCommandPool;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "");
        return cmd;
    }

    void VulkanContext::endSingleTimeCommands(VkCommandBuffer cmd,
                                              VkCommandPool pool, VkQueue queue)
    {
        if(pool == VK_NULL_HANDLE) pool = graphicsCommandPool;
        if(queue == VK_NULL_HANDLE) queue = graphicsQueue;

        VK_CHECK(vkEndCommandBuffer(cmd), "");

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "");
        VK_CHECK(vkQueueWaitIdle(queue), "");

        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    bool VulkanContext::checkValidationLayerSupport() {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

        for(const char* name : validationLayers) {
            bool found = false;
            for(const auto& layer : layers) {
                if(strcmp(name, layer.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if(!found) return false;
        }
        return true;
    }

    std::vector<const char*> VulkanContext::getRequiredExtensions() {
        Uint32 sdlCount = 0;
        char const* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);

        std::vector<const char*> extensions;
        for(Uint32 i = 0; i < sdlCount; i++) {
            extensions.push_back(sdlExts[i]);
        }

        if(enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

} // namespace vpbr