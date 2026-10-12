// src/renderer/renderer.h
#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>

#include "renderer/swapchain.h"
#include "renderer/vulkancontext.h"
#include "common/types.h"
#include "scene/scene.h"
#include "memory/buffer.h"

namespace vpbr {

    class Renderer {
    public:
        void init(VulkanContext& ctx, Swapchain& swapchain, Scene& scene);
        void cleanup(VulkanContext& ctx);
        void recreateSwapchain(VulkanContext& ctx, Swapchain& swapchain,
                               uint32_t width, uint32_t height);
        void update(Scene& scene, float deltaTime);
        void drawFrame(VulkanContext& ctx, Swapchain& swapchain, Scene& scene);
        void handleInput(const SDL_Event& event, float deltaTime);

    private:
        // Render passes
        void createRenderPass(VulkanContext& ctx, Swapchain& swapchain);
        void createShadowRenderPass(VulkanContext& ctx);
        void createShadowResources(VulkanContext& ctx);

        // Pipeline
        void createDescriptorSetLayouts(VulkanContext& ctx);
        void createPipelines(VulkanContext& ctx, Swapchain& swapchain);
        void createShadowPipeline(VulkanContext& ctx);

        // Framebuffer and sync
        void createFramebuffers(VulkanContext& ctx, Swapchain& swapchain);
        void createCommandBuffers(VulkanContext& ctx);
        void createSyncObjects(VulkanContext& ctx);
        void createUniformBuffers(VulkanContext& ctx, size_t swapchainImageCount);

        // Descriptor sets
        void createDescriptorPool(VulkanContext& ctx, size_t swapchainImageCount);
        void createGlobalDescriptorSets(VulkanContext& ctx, size_t swapchainImageCount);
        void createShadowDescriptorSets(VulkanContext& ctx);

        // Per-frame data
        void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex,
                                 Swapchain& swapchain, Scene& scene);
        void recordShadowPass(VkCommandBuffer cmd, Scene& scene);
        void updateUniformBuffers(uint32_t currentImage, Scene& scene);

        // Vulkan objects
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPass shadowRenderPass = VK_NULL_HANDLE;

        VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout materialSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout shadowSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        VkPipeline shadowPipeline = VK_NULL_HANDLE;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> globalDescriptorSets;
        VkDescriptorSet shadowDescriptorSet = VK_NULL_HANDLE;

        std::vector<VkFramebuffer> framebuffers;
        VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;

        // Shadow map resources
        Image shadowMapImage;
        VkSampler shadowSampler = VK_NULL_HANDLE;

        // Uniform buffers
        std::vector<Buffer> cameraUBOs;
        std::vector<Buffer> lightsUBOs;
        Buffer shadowLightSpaceUBO;

        // Sync objects
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;

        // Command buffers (per swapchain image)
        std::vector<VkCommandBuffer> commandBuffers;

        // Camera state
        glm::vec3 cameraPos = glm::vec3(0.0f, 1.5f, 5.0f);
        glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        float cameraYaw = -90.0f;
        float cameraPitch = 0.0f;
        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;

        // Frame tracking
        uint32_t currentFrame = 0;
        bool framebufferResized = false;

        // Light space matrix
        glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
    };

} // namespace vpbr