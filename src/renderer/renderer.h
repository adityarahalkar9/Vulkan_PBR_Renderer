#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <vector>

#include "renderer/swapchain.h"
#include "renderer/vulkancontext.h"
#include "common/types.h"
#include "scene/scene.h"
#include "memory/buffer.h"
#include "memory/image.h"

namespace vpbr {

    class Renderer {
    public:
        void init(VulkanContext& ctx, Swapchain& swapchain, Scene& scene);
        void cleanup(VulkanContext& ctx);
        void recreateSwapchain(VulkanContext& ctx, Swapchain& swapchain,
                               uint32_t width, uint32_t height);
        void update(Scene& scene, float deltaTime);
        void drawFrame(VulkanContext& ctx, Swapchain& swapchain, Scene& scene);
        void handleInput(const SDL_Event& event, Camera& camera);

    private:
        void createRenderPass(VulkanContext& ctx, Swapchain& swapchain);
        void createShadowRenderPass(VulkanContext& ctx);
        void createDescriptorSetLayouts(VulkanContext& ctx);
        void createDefaultTextures(VulkanContext& ctx);
        void createShadowResources(VulkanContext& ctx);
        void createPipelines(VulkanContext& ctx, Swapchain& swapchain);
        void createShadowPipeline(VulkanContext& ctx);
        void createFramebuffers(VulkanContext& ctx, Swapchain& swapchain);
        void createUniformBuffers(VulkanContext& ctx, size_t imageCount);
        void createDescriptorPool(VulkanContext& ctx, size_t imageCount);
        void createGlobalDescriptorSets(VulkanContext& ctx, size_t imageCount);
        void createShadowSamplerDescriptorSet(VulkanContext& ctx);
        void createShadowPassDescriptorSet(VulkanContext& ctx);
        void createMaterialDescriptorSets(VulkanContext& ctx, Scene& scene);
        void createCommandBuffers(VulkanContext& ctx);
        void createSyncObjects(VulkanContext& ctx);

        void updateUniformBuffers(VulkanContext& ctx, uint32_t imageIdx,
                                  Scene& scene, float aspect);
        void recordShadowPass(VkCommandBuffer cmd, Scene& scene);
        void recordMainPass(VkCommandBuffer cmd, uint32_t imageIdx,
                            Swapchain& swapchain, Scene& scene);

        // Render passes
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPass shadowRenderPass = VK_NULL_HANDLE;

        // Descriptor layouts
        VkDescriptorSetLayout globalSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout materialSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout shadowSamplerSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout shadowPassSetLayout = VK_NULL_HANDLE;

        // Pipelines
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline = VK_NULL_HANDLE;
        VkPipeline shadowPipeline = VK_NULL_HANDLE;

        // Descriptors
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> globalDescriptorSets;
        VkDescriptorSet shadowSamplerDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet shadowPassDescriptorSet = VK_NULL_HANDLE;

        // Framebuffers
        std::vector<VkFramebuffer> framebuffers;
        VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;

        // Shadow map
        Image shadowMapImage;
        VkSampler shadowSampler = VK_NULL_HANDLE;

        // Uniform buffers
        std::vector<Buffer> cameraUBOs;
        std::vector<Buffer> lightsUBOs;
        Buffer shadowLightSpaceUBO;

        // Default textures
        Image defaultWhiteTex;
        Image defaultNormalTex;
        Image defaultBlackTex;

        // Sync
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;

        // Commands
        std::vector<VkCommandBuffer> commandBuffers;

        // Input
        bool mouseCaptured = false;
        bool keys[SDL_SCANCODE_COUNT] = {};

        // Frame
        uint32_t currentFrame = 0;
        bool framebufferResized = false;

        glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
    };

} // namespace vpbr