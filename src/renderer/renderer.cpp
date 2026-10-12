// src/renderer/renderer.cpp
#include "renderer/renderer.h"
#include "renderer/vulkancontext.h"
#include "renderer/swapchain.h"
#include "scene/scene.h"
#include "utils/logger.h"
#include "utils/vulkanutils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

namespace vpbr {

    void Renderer::init(VulkanContext& ctx, Swapchain& swapchain, Scene& scene) {
        LOG_INFO("Initializing renderer...");
        createShadowRenderPass(ctx);
        createRenderPass(ctx, swapchain);
        createDescriptorSetLayouts(ctx);
        createPipelines(ctx, swapchain);
        createShadowPipeline(ctx);
        createShadowResources(ctx);
        createFramebuffers(ctx, swapchain);
        createUniformBuffers(ctx, swapchain.images.size());
        createDescriptorPool(ctx, swapchain.images.size());
        createGlobalDescriptorSets(ctx, swapchain.images.size());
        createShadowDescriptorSets(ctx);
        createCommandBuffers(ctx);
        createSyncObjects(ctx);
        LOG_INFO("Renderer initialized.");
    }

    void Renderer::cleanup(VulkanContext& ctx) {
        ctx.waitIdle();

        for(auto& ubo : cameraUBOs) ubo.destroy(ctx.allocator);
        for(auto& ubo : lightsUBOs) ubo.destroy(ctx.allocator);
        shadowLightSpaceUBO.destroy(ctx.allocator);

        if(shadowSampler != VK_NULL_HANDLE) {
            vkDestroySampler(ctx.device, shadowSampler, nullptr);
            shadowSampler = VK_NULL_HANDLE;
        }
        shadowMapImage.destroy(ctx.allocator, ctx.device);

        for(auto fb : framebuffers) {
            if(fb != VK_NULL_HANDLE) vkDestroyFramebuffer(ctx.device, fb, nullptr);
        }
        framebuffers.clear();

        if(shadowFramebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(ctx.device, shadowFramebuffer, nullptr);
            shadowFramebuffer = VK_NULL_HANDLE;
        }

        if(descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        if(shadowPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx.device, shadowPipeline, nullptr);
            shadowPipeline = VK_NULL_HANDLE;
        }
        if(graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx.device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }

        if(shadowPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx.device, shadowPipelineLayout, nullptr);
            shadowPipelineLayout = VK_NULL_HANDLE;
        }
        if(pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }

        if(shadowSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx.device, shadowSetLayout, nullptr);
            shadowSetLayout = VK_NULL_HANDLE;
        }
        if(materialSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx.device, materialSetLayout, nullptr);
            materialSetLayout = VK_NULL_HANDLE;
        }
        if(globalSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx.device, globalSetLayout, nullptr);
            globalSetLayout = VK_NULL_HANDLE;
        }

        if(shadowRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(ctx.device, shadowRenderPass, nullptr);
            shadowRenderPass = VK_NULL_HANDLE;
        }
        if(renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(ctx.device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if(imageAvailableSemaphores[i] != VK_NULL_HANDLE)
                vkDestroySemaphore(ctx.device, imageAvailableSemaphores[i], nullptr);
            if(renderFinishedSemaphores[i] != VK_NULL_HANDLE)
                vkDestroySemaphore(ctx.device, renderFinishedSemaphores[i], nullptr);
            if(inFlightFences[i] != VK_NULL_HANDLE)
                vkDestroyFence(ctx.device, inFlightFences[i], nullptr);
        }
        imageAvailableSemaphores.clear();
        renderFinishedSemaphores.clear();
        inFlightFences.clear();
        imagesInFlight.clear();
    }

    void Renderer::recreateSwapchain(VulkanContext& ctx, Swapchain& swapchain,
                                     uint32_t width, uint32_t height) {
        ctx.waitIdle();

        for(auto fb : framebuffers) {
            if(fb != VK_NULL_HANDLE) vkDestroyFramebuffer(ctx.device, fb, nullptr);
        }
        framebuffers.clear();

        for(auto& ubo : cameraUBOs) ubo.destroy(ctx.allocator);
        for(auto& ubo : lightsUBOs) ubo.destroy(ctx.allocator);
        cameraUBOs.clear();
        lightsUBOs.clear();

        if(descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        if(graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx.device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        if(pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if(renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(ctx.device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        swapchain.recreate(ctx, width, height);

        createRenderPass(ctx, swapchain);
        createPipelines(ctx, swapchain);
        createFramebuffers(ctx, swapchain);
        createUniformBuffers(ctx, swapchain.images.size());
        createDescriptorPool(ctx, swapchain.images.size());
        createGlobalDescriptorSets(ctx, swapchain.images.size());
        createCommandBuffers(ctx);
    }

    void Renderer::createRenderPass(VulkanContext& ctx, Swapchain& swapchain) {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain.imageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = swapchain.depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = static_cast<uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(ctx.device, &info, nullptr, &renderPass),
                 "Failed to create render pass");
    }

    void Renderer::createShadowRenderPass(VulkanContext& ctx) {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat(ctx.physicalDevice);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkAttachmentReference depthRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &depthAttachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(ctx.device, &info, nullptr, &shadowRenderPass),
                 "Failed to create shadow render pass");
    }

    void Renderer::createShadowResources(VulkanContext& ctx) {
        // Create shadow map image
        VkFormat depthFormat = findDepthFormat(ctx.physicalDevice);
        shadowMapImage.create(ctx.allocator, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
                              depthFormat,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_IMAGE_ASPECT_DEPTH_BIT);
        shadowMapImage.createView(ctx.device, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VK_CHECK(vkCreateSampler(ctx.device, &samplerInfo, nullptr, &shadowSampler),
                 "Failed to create shadow sampler");

        // Shadow framebuffer
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = shadowRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &shadowMapImage.view;
        fbInfo.width = SHADOW_MAP_SIZE;
        fbInfo.height = SHADOW_MAP_SIZE;
        fbInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(ctx.device, &fbInfo, nullptr, &shadowFramebuffer),
                 "Failed to create shadow framebuffer");

        // Light space UBO for shadow pass
        shadowLightSpaceUBO.create(ctx.allocator, sizeof(glm::mat4),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    void Renderer::createFramebuffers(VulkanContext& ctx, Swapchain& swapchain) {
        framebuffers.resize(swapchain.imageViews.size());

        for(size_t i = 0; i < swapchain.imageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapchain.imageViews[i],
                swapchain.depthImageView
            };

            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass;
            info.attachmentCount = static_cast<uint32_t>(attachments.size());
            info.pAttachments = attachments.data();
            info.width = swapchain.extent.width;
            info.height = swapchain.extent.height;
            info.layers = 1;

            VK_CHECK(vkCreateFramebuffer(ctx.device, &info, nullptr, &framebuffers[i]),
                     "Failed to create framebuffer");
        }
    }

    void Renderer::createCommandBuffers(VulkanContext& ctx) {
        commandBuffers.resize(framebuffers.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx.graphicsCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        VK_CHECK(vkAllocateCommandBuffers(ctx.device, &allocInfo, commandBuffers.data()),
                 "Failed to allocate command buffers");
    }

    void Renderer::createSyncObjects(VulkanContext& ctx) {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(commandBuffers.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreInfo, nullptr,
                                       &imageAvailableSemaphores[i]),
                     "Failed to create image available semaphore");
            VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreInfo, nullptr,
                                       &renderFinishedSemaphores[i]),
                     "Failed to create render finished semaphore");
            VK_CHECK(vkCreateFence(ctx.device, &fenceInfo, nullptr, &inFlightFences[i]),
                     "Failed to create in-flight fence");
        }
    }

    // ... [remaining methods for pipeline creation, descriptor sets,
    //      uniform buffer updates, command recording, draw frame,
    //      and shadow pass would follow here - approximately 800 more lines]

    void Renderer::update(Scene& scene, float deltaTime) {
        // Update camera matrices
        // Update light positions and light space matrix
        // Update scene transforms
    }

    void Renderer::drawFrame(VulkanContext& ctx, Swapchain& swapchain, Scene& scene) {
        vkWaitForFences(ctx.device, 1, &inFlightFences[currentFrame],
                        VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(ctx.device, swapchain.swapchain,
                                                UINT64_MAX,
                                                imageAvailableSemaphores[currentFrame],
                                                VK_NULL_HANDLE, &imageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain(ctx, swapchain, swapchain.extent.width, swapchain.extent.height);
            return;
        }
        else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swapchain image");
        }

        if(imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(ctx.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        updateUniformBuffers(imageIndex, scene);

        vkResetFences(ctx.device, 1, &inFlightFences[currentFrame]);

        // Record and submit commands
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex, swapchain, scene);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo,
                               inFlightFences[currentFrame]),
                 "Failed to submit draw command buffer");

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(ctx.presentQueue, &presentInfo);

        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
           framebufferResized) {
            framebufferResized = false;
            recreateSwapchain(ctx, swapchain, swapchain.extent.width, swapchain.extent.height);
        }
        else if(result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

} // namespace vpbr