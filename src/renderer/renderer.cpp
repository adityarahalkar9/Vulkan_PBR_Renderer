#include "renderer/renderer.h"
#include "renderer/texture_loader.h"
#include "utils/logger.h"
#include "utils/vulkanutils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <array>

namespace vpbr {

    // ═════════════════ INIT / CLEANUP ═════════════════

    void Renderer::init(VulkanContext& ctx, Swapchain& swapchain, Scene& scene) {
        LOG_INFO("Initializing renderer...");

        createShadowRenderPass(ctx);
        createRenderPass(ctx, swapchain);
        createDescriptorSetLayouts(ctx);
        createDefaultTextures(ctx);
        createShadowPipeline(ctx);
        createPipelines(ctx, swapchain);
        createShadowResources(ctx);
        createFramebuffers(ctx, swapchain);
        createUniformBuffers(ctx, swapchain.images.size());
        createDescriptorPool(ctx, swapchain.images.size());
        createGlobalDescriptorSets(ctx, swapchain.images.size());
        createShadowSamplerDescriptorSet(ctx);
        createShadowPassDescriptorSet(ctx);
        createMaterialDescriptorSets(ctx, scene);
        createCommandBuffers(ctx);
        createSyncObjects(ctx);

        LOG_INFO("Renderer initialized.");
    }

    void Renderer::cleanup(VulkanContext& ctx) {
        ctx.waitIdle();

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

        for(auto& ubo : cameraUBOs) ubo.destroy(ctx.allocator);
        for(auto& ubo : lightsUBOs) ubo.destroy(ctx.allocator);
        shadowLightSpaceUBO.destroy(ctx.allocator);
        cameraUBOs.clear();
        lightsUBOs.clear();

        if(shadowSampler != VK_NULL_HANDLE) {
            vkDestroySampler(ctx.device, shadowSampler, nullptr);
            shadowSampler = VK_NULL_HANDLE;
        }
        shadowMapImage.destroy(ctx.allocator, ctx.device);

        defaultWhiteTex.destroy(ctx.allocator, ctx.device);
        defaultNormalTex.destroy(ctx.allocator, ctx.device);
        defaultBlackTex.destroy(ctx.allocator, ctx.device);

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

        if(shadowPassSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx.device, shadowPassSetLayout, nullptr);
            shadowPassSetLayout = VK_NULL_HANDLE;
        }
        if(shadowSamplerSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx.device, shadowSamplerSetLayout, nullptr);
            shadowSamplerSetLayout = VK_NULL_HANDLE;
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

        LOG_INFO("Renderer cleaned up.");
    }

    void Renderer::recreateSwapchain(VulkanContext& ctx, Swapchain& swapchain,
                                     uint32_t width, uint32_t height)
    {
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
        createShadowSamplerDescriptorSet(ctx);
        createShadowPassDescriptorSet(ctx);
        createCommandBuffers(ctx);
    }

    // ═════════════════ INPUT & UPDATE ═════════════════

    void Renderer::handleInput(const SDL_Event& event, Camera& camera) {
        switch(event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                {
                    bool down = (event.type == SDL_EVENT_KEY_DOWN);
                    keys[event.key.scancode] = down;
                    break;
                }

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if(event.button.button == SDL_BUTTON_LEFT) mouseCaptured = true;
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if(event.button.button == SDL_BUTTON_LEFT) mouseCaptured = false;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if(mouseCaptured) {
                    camera.yaw += event.motion.xrel * camera.mouseSensitivity;
                    camera.pitch -= event.motion.yrel * camera.mouseSensitivity;
                    camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
                    camera.updateCameraVectors();
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                framebufferResized = true;
                break;

            default:
                break;
        }
    }

    void Renderer::update(Scene& scene, float dt) {
        float speed = scene.camera.movementSpeed * dt;

        if(keys[SDL_SCANCODE_W]) scene.camera.position += scene.camera.front * speed;
        if(keys[SDL_SCANCODE_S]) scene.camera.position -= scene.camera.front * speed;
        if(keys[SDL_SCANCODE_A]) scene.camera.position -= scene.camera.right * speed;
        if(keys[SDL_SCANCODE_D]) scene.camera.position += scene.camera.right * speed;
        if(keys[SDL_SCANCODE_SPACE]) scene.camera.position += scene.camera.up * speed;
        if(keys[SDL_SCANCODE_LSHIFT]) scene.camera.position -= scene.camera.up * speed;

        scene.camera.updateCameraVectors();
    }

    // ═════════════════ DRAW FRAME ═════════════════

    void Renderer::drawFrame(VulkanContext& ctx, Swapchain& swapchain, Scene& scene) {
        vkWaitForFences(ctx.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            ctx.device, swapchain.swapchain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR) return;
        if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swapchain image");
        }

        if(imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(ctx.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        float aspect = swapchain.extent.width /
            static_cast<float>(swapchain.extent.height);
        updateUniformBuffers(ctx, imageIndex, scene, aspect);

        vkResetFences(ctx.device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);

        // Record commands
        VkCommandBuffer cmd = commandBuffers[imageIndex];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "");

        recordShadowPass(cmd, scene);
        recordMainPass(cmd, imageIndex, swapchain, scene);

        VK_CHECK(vkEndCommandBuffer(cmd), "");

        // Submit
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSems[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = waitSems;
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        VkSemaphore signalSems[] = {renderFinishedSemaphores[currentFrame]};
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = signalSems;

        VK_CHECK(vkQueueSubmit(ctx.graphicsQueue, 1, &submit,
                               inFlightFences[currentFrame]), "");

        // Present
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = signalSems;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain.swapchain;
        present.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(ctx.presentQueue, &present);

        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
           framebufferResized)
        {
            framebufferResized = false;
        }
        else if(result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ═════════════════ RENDER PASSES ═════════════════

    void Renderer::createRenderPass(VulkanContext& ctx, Swapchain& swapchain) {
        VkAttachmentDescription color{};
        color.format = swapchain.imageFormat;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth{};
        depth.format = swapchain.depthFormat;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {color, depth};

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dep;

        VK_CHECK(vkCreateRenderPass(ctx.device, &info, nullptr, &renderPass), "");
    }

    void Renderer::createShadowRenderPass(VulkanContext& ctx) {
        VkAttachmentDescription depth{};
        depth.format = findDepthFormat(ctx.physicalDevice);
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkAttachmentReference depthRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &depth;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dep;

        VK_CHECK(vkCreateRenderPass(ctx.device, &info, nullptr, &shadowRenderPass), "");
    }

    // ═════════════════ DESCRIPTOR LAYOUTS ═════════════════

    void Renderer::createDescriptorSetLayouts(VulkanContext& ctx) {
        // Set 0: global (camera + lights)
        {
            std::array<VkDescriptorSetLayoutBinding, 2> b{};

            b[0].binding = 0;
            b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b[0].descriptorCount = 1;
            b[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            b[1].binding = 1;
            b[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b[1].descriptorCount = 1;
            b[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 2;
            info.pBindings = b.data();

            VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, nullptr,
                                                 &globalSetLayout), "");
        }

        // Set 1: material (5 textures + 1 UBO)
        {
            std::array<VkDescriptorSetLayoutBinding, 6> b{};

            for(uint32_t i = 0; i < 5; i++) {
                b[i].binding = i;
                b[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                b[i].descriptorCount = 1;
                b[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            b[5].binding = 5;
            b[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b[5].descriptorCount = 1;
            b[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 6;
            info.pBindings = b.data();

            VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, nullptr,
                                                 &materialSetLayout), "");
        }

        // Set 2: shadow map sampler (for PBR)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = 0;
            b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 1;
            info.pBindings = &b;

            VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, nullptr,
                                                 &shadowSamplerSetLayout), "");
        }

        // Shadow pass: lightSpaceMatrix UBO only
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = 0;
            b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 1;
            info.pBindings = &b;

            VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, nullptr,
                                                 &shadowPassSetLayout), "");
        }
    }

    // ═════════════════ DEFAULT TEXTURES ═════════════════

    void Renderer::createDefaultTextures(VulkanContext& ctx) {
        TextureLoader::createSolidColor(glm::vec4(1.0f), defaultWhiteTex, ctx);
        TextureLoader::createSolidColor(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f),
                                        defaultNormalTex, ctx);
        TextureLoader::createSolidColor(glm::vec4(0.0f), defaultBlackTex, ctx);
    }

    // ═════════════════ SHADOW RESOURCES ═════════════════

    void Renderer::createShadowResources(VulkanContext& ctx) {
        VkFormat depthFormat = findDepthFormat(ctx.physicalDevice);

        shadowMapImage.create(ctx.allocator, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
                              depthFormat,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT, 1);
        shadowMapImage.createView(ctx.device, depthFormat,
                                  VK_IMAGE_ASPECT_DEPTH_BIT, 1);

        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_LINEAR;
        si.minFilter = VK_FILTER_LINEAR;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.anisotropyEnable = VK_FALSE;
        si.compareEnable = VK_FALSE;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.maxLod = 1.0f;

        VK_CHECK(vkCreateSampler(ctx.device, &si, nullptr, &shadowSampler), "");

        VkFramebufferCreateInfo fbi{};
        fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass = shadowRenderPass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &shadowMapImage.view;
        fbi.width = SHADOW_MAP_SIZE;
        fbi.height = SHADOW_MAP_SIZE;
        fbi.layers = 1;

        VK_CHECK(vkCreateFramebuffer(ctx.device, &fbi, nullptr, &shadowFramebuffer), "");

        shadowLightSpaceUBO.create(ctx.allocator, sizeof(glm::mat4),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    // ═════════════════ PIPELINES ═════════════════

    void Renderer::createPipelines(VulkanContext& ctx, Swapchain& swapchain) {
        auto vertCode = readShaderFile("shaders/pbr.vert.spv");
        auto fragCode = readShaderFile("shaders/pbr.frag.spv");

        VkShaderModule vertModule = createShaderModule(ctx.device, vertCode);
        VkShaderModule fragModule = createShaderModule(ctx.device, fragCode);

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        auto bindingDesc = Vertex::getBindingDescription();
        auto attrDescs = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attrDescs.size());
        vertexInput.pVertexAttributeDescriptions = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1;
        vs.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.lineWidth = 1.0f;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = 0xF;
        cba.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &cba;

        std::array<VkDynamicState, 2> dyn = {VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy{};
        dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dy.dynamicStateCount = 2;
        dy.pDynamicStates = dyn.data();

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.size = sizeof(ModelPushConstant);

        std::array<VkDescriptorSetLayout, 3> layouts = {
            globalSetLayout, materialSetLayout, shadowSamplerSetLayout
        };

        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 3;
        pli.pSetLayouts = layouts.data();
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &pushRange;

        VK_CHECK(vkCreatePipelineLayout(ctx.device, &pli, nullptr, &pipelineLayout), "");

        VkGraphicsPipelineCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.stageCount = 2;
        pi.pStages = stages;
        pi.pVertexInputState = &vertexInput;
        pi.pInputAssemblyState = &ia;
        pi.pViewportState = &vs;
        pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms;
        pi.pDepthStencilState = &ds;
        pi.pColorBlendState = &cb;
        pi.pDynamicState = &dy;
        pi.layout = pipelineLayout;
        pi.renderPass = renderPass;
        pi.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pi,
                                           nullptr, &graphicsPipeline), "");

        vkDestroyShaderModule(ctx.device, vertModule, nullptr);
        vkDestroyShaderModule(ctx.device, fragModule, nullptr);
        (void)swapchain;
    }

    void Renderer::createShadowPipeline(VulkanContext& ctx) {
        auto vertCode = readShaderFile("shaders/shadow.vert.spv");
        auto fragCode = readShaderFile("shaders/shadow.frag.spv");

        VkShaderModule vertModule = createShaderModule(ctx.device, vertCode);
        VkShaderModule fragModule = createShaderModule(ctx.device, fragCode);

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attr{};
        attr.binding = 0;
        attr.location = 0;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = offsetof(Vertex, position);

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = &attr;

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1;
        vs.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.depthClampEnable = VK_TRUE;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.lineWidth = 1.0f;
        rs.cullMode = VK_CULL_MODE_FRONT_BIT;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.depthBiasEnable = VK_TRUE;
        rs.depthBiasConstantFactor = 1.25f;
        rs.depthBiasSlopeFactor = 1.75f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 0;

        std::array<VkDynamicState, 2> dyn = {VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy{};
        dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dy.dynamicStateCount = 2;
        dy.pDynamicStates = dyn.data();

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.size = sizeof(ModelPushConstant);

        VkDescriptorSetLayout setLayouts[] = {shadowPassSetLayout};

        VkPipelineLayoutCreateInfo pli{};
        pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pli.setLayoutCount = 1;
        pli.pSetLayouts = setLayouts;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &pushRange;

        VK_CHECK(vkCreatePipelineLayout(ctx.device, &pli, nullptr,
                                        &shadowPipelineLayout), "");

        VkGraphicsPipelineCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.stageCount = 2;
        pi.pStages = stages;
        pi.pVertexInputState = &vertexInput;
        pi.pInputAssemblyState = &ia;
        pi.pViewportState = &vs;
        pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms;
        pi.pDepthStencilState = &ds;
        pi.pColorBlendState = &cb;
        pi.pDynamicState = &dy;
        pi.layout = shadowPipelineLayout;
        pi.renderPass = shadowRenderPass;
        pi.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pi,
                                           nullptr, &shadowPipeline), "");

        vkDestroyShaderModule(ctx.device, vertModule, nullptr);
        vkDestroyShaderModule(ctx.device, fragModule, nullptr);
    }

    // ═════════════════ FRAMEBUFFERS ═════════════════

    void Renderer::createFramebuffers(VulkanContext& ctx, Swapchain& swapchain) {
        framebuffers.resize(swapchain.imageViews.size());

        for(size_t i = 0; i < swapchain.imageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapchain.imageViews[i], swapchain.depthImageView
            };

            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass;
            info.attachmentCount = 2;
            info.pAttachments = attachments.data();
            info.width = swapchain.extent.width;
            info.height = swapchain.extent.height;
            info.layers = 1;

            VK_CHECK(vkCreateFramebuffer(ctx.device, &info, nullptr,
                                         &framebuffers[i]), "");
        }
    }

    // ═════════════════ UNIFORM BUFFERS ═════════════════

    void Renderer::createUniformBuffers(VulkanContext& ctx, size_t imageCount) {
        cameraUBOs.resize(imageCount);
        lightsUBOs.resize(imageCount);

        for(size_t i = 0; i < imageCount; i++) {
            cameraUBOs[i].create(ctx.allocator, sizeof(CameraUBO),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
            lightsUBOs[i].create(ctx.allocator, sizeof(LightsUBO),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
    }

    void Renderer::updateUniformBuffers(VulkanContext& ctx, uint32_t imageIdx,
                                        Scene& scene, float aspect)
    {
        // Camera
        {
            CameraUBO cam;
            cam.view = scene.camera.getViewMatrix();
            cam.proj = scene.camera.getProjectionMatrix(aspect);
            cam.viewProj = cam.proj * cam.view;
            cam.cameraPos = glm::vec4(scene.camera.position, 1.0f);

            void* data = cameraUBOs[imageIdx].map(ctx.allocator);
            std::memcpy(data, &cam, sizeof(CameraUBO));
            cameraUBOs[imageIdx].unmap(ctx.allocator);
        }

        // Lights
        {
            LightsUBO lubo{};
            lubo.numLights = std::min(static_cast<int>(scene.lights.size()),
                                      static_cast<int>(MAX_LIGHTS));

            // Update light space matrix for shadow
            if(!scene.lights.empty()) {
                lightSpaceMatrix = scene.computeLightSpaceMatrix(0);
                scene.lights[0].lightSpaceMatrix = lightSpaceMatrix;

                void* data = shadowLightSpaceUBO.map(ctx.allocator);
                std::memcpy(data, &lightSpaceMatrix, sizeof(glm::mat4));
                shadowLightSpaceUBO.unmap(ctx.allocator);
            }

            for(int i = 0; i < lubo.numLights; i++) {
                lubo.lights[i].position = scene.lights[i].position;
                lubo.lights[i].direction = scene.lights[i].direction;
                lubo.lights[i].color = scene.lights[i].color;
                lubo.lights[i].lightSpaceMatrix = scene.lights[i].lightSpaceMatrix;
            }

            void* data = lightsUBOs[imageIdx].map(ctx.allocator);
            std::memcpy(data, &lubo, sizeof(LightsUBO));
            lightsUBOs[imageIdx].unmap(ctx.allocator);
        }
    }

    // ═════════════════ DESCRIPTOR POOL & SETS ═════════════════

    void Renderer::createDescriptorPool(VulkanContext& ctx, size_t imageCount) {
        std::array<VkDescriptorPoolSize, 2> sizes{};

        sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sizes[0].descriptorCount = static_cast<uint32_t>(imageCount * 2 + 100);

        sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizes[1].descriptorCount = 200;

        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.poolSizeCount = 2;
        info.pPoolSizes = sizes.data();
        info.maxSets = static_cast<uint32_t>(imageCount * 2 + 200);

        VK_CHECK(vkCreateDescriptorPool(ctx.device, &info, nullptr, &descriptorPool), "");
    }

    void Renderer::createGlobalDescriptorSets(VulkanContext& ctx, size_t imageCount) {
        globalDescriptorSets.resize(imageCount);

        std::vector<VkDescriptorSetLayout> layouts(imageCount, globalSetLayout);
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = descriptorPool;
        ai.descriptorSetCount = static_cast<uint32_t>(imageCount);
        ai.pSetLayouts = layouts.data();

        VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai,
                                          globalDescriptorSets.data()), "");

        for(size_t i = 0; i < imageCount; i++) {
            VkDescriptorBufferInfo camInfo{cameraUBOs[i].buffer, 0, sizeof(CameraUBO)};
            VkDescriptorBufferInfo lightInfo{lightsUBOs[i].buffer, 0, sizeof(LightsUBO)};

            VkWriteDescriptorSet writes[2] = {};

            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = globalDescriptorSets[i];
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo = &camInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = globalDescriptorSets[i];
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[1].descriptorCount = 1;
            writes[1].pBufferInfo = &lightInfo;

            vkUpdateDescriptorSets(ctx.device, 2, writes, 0, nullptr);
        }
    }

    void Renderer::createShadowSamplerDescriptorSet(VulkanContext& ctx) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = descriptorPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &shadowSamplerSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai,
                                          &shadowSamplerDescriptorSet), "");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfo.imageView = shadowMapImage.view;
        imageInfo.sampler = shadowSampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = shadowSamplerDescriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
    }

    void Renderer::createShadowPassDescriptorSet(VulkanContext& ctx) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = descriptorPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &shadowPassSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai,
                                          &shadowPassDescriptorSet), "");

        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = shadowLightSpaceUBO.buffer;
        bufInfo.offset = 0;
        bufInfo.range = sizeof(glm::mat4);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = shadowPassDescriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufInfo;

        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
    }

    void Renderer::createMaterialDescriptorSets(VulkanContext& ctx, Scene& scene) {
        for(auto& mat : scene.materials) {
            // Create material UBO
            mat.paramsUBO.create(ctx.allocator, sizeof(PBRMaterialParams),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
            mat.paramsUBO.upload(ctx.allocator, &mat.params,
                                 sizeof(PBRMaterialParams));

            VkDescriptorSetAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool = descriptorPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &materialSetLayout;

            VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai,
                                              &mat.descriptorSet), "");

            std::array<VkDescriptorImageInfo, 5> images{};

            auto setup = [&](int i, const Image& tex, const Image& fallback) {
                images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                images[i].imageView = tex.view != VK_NULL_HANDLE ?
                    tex.view : fallback.view;
                images[i].sampler = tex.sampler != VK_NULL_HANDLE ?
                    tex.sampler : fallback.sampler;
                };

            setup(0, mat.baseColorMap, defaultWhiteTex);
            setup(1, mat.metallicRoughnessMap, defaultWhiteTex);
            setup(2, mat.normalMap, defaultNormalTex);
            setup(3, mat.occlusionMap, defaultWhiteTex);
            setup(4, mat.emissiveMap, defaultBlackTex);

            VkDescriptorBufferInfo matUBOInfo{};
            matUBOInfo.buffer = mat.paramsUBO.buffer;
            matUBOInfo.offset = 0;
            matUBOInfo.range = sizeof(PBRMaterialParams);

            std::array<VkWriteDescriptorSet, 6> writes{};

            for(int i = 0; i < 5; i++) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = mat.descriptorSet;
                writes[i].dstBinding = i;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[i].descriptorCount = 1;
                writes[i].pImageInfo = &images[i];
            }

            writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[5].dstSet = mat.descriptorSet;
            writes[5].dstBinding = 5;
            writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[5].descriptorCount = 1;
            writes[5].pBufferInfo = &matUBOInfo;

            vkUpdateDescriptorSets(ctx.device, 6, writes.data(), 0, nullptr);
        }
    }

    // ═════════════════ COMMANDS & SYNC ═════════════════

    void Renderer::createCommandBuffers(VulkanContext& ctx) {
        commandBuffers.resize(framebuffers.size());

        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = ctx.graphicsCommandPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        VK_CHECK(vkAllocateCommandBuffers(ctx.device, &ai, commandBuffers.data()), "");
    }

    void Renderer::createSyncObjects(VulkanContext& ctx) {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(commandBuffers.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VK_CHECK(vkCreateSemaphore(ctx.device, &si, nullptr,
                                       &imageAvailableSemaphores[i]), "");
            VK_CHECK(vkCreateSemaphore(ctx.device, &si, nullptr,
                                       &renderFinishedSemaphores[i]), "");
            VK_CHECK(vkCreateFence(ctx.device, &fi, nullptr,
                                   &inFlightFences[i]), "");
        }
    }

    // ═════════════════ RECORD COMMANDS ═════════════════

    void Renderer::recordShadowPass(VkCommandBuffer cmd, Scene& scene) {
        VkClearValue clear{};
        clear.depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass = shadowRenderPass;
        rp.framebuffer = shadowFramebuffer;
        rp.renderArea.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        rp.clearValueCount = 1;
        rp.pClearValues = &clear;

        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

        VkViewport vp{};
        vp.width = static_cast<float>(SHADOW_MAP_SIZE);
        vp.height = static_cast<float>(SHADOW_MAP_SIZE);
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE};
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                shadowPipelineLayout, 0, 1,
                                &shadowPassDescriptorSet, 0, nullptr);

        for(const auto& mesh : scene.meshes) {
            ModelPushConstant push;
            push.model = mesh.transform.getMatrix();
            vkCmdPushConstants(cmd, shadowPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(ModelPushConstant), &push);

            VkBuffer vb[] = {mesh.vertexBuffer.buffer};
            VkDeviceSize offs[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vb, offs);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

    void Renderer::recordMainPass(VkCommandBuffer cmd, uint32_t imageIdx,
                                  Swapchain& swapchain, Scene& scene)
    {
        std::array<VkClearValue, 2> clears{};
        clears[0].color = {{0.05f, 0.05f, 0.08f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass = renderPass;
        rp.framebuffer = framebuffers[imageIdx];
        rp.renderArea.extent = swapchain.extent;
        rp.clearValueCount = 2;
        rp.pClearValues = clears.data();

        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkViewport vp{};
        vp.width = static_cast<float>(swapchain.extent.width);
        vp.height = static_cast<float>(swapchain.extent.height);
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.extent = swapchain.extent;
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1,
                                &globalDescriptorSets[currentFrame], 0, nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 2, 1,
                                &shadowSamplerDescriptorSet, 0, nullptr);

        for(const auto& mesh : scene.meshes) {
            if(mesh.materialIndex >= 0 &&
               mesh.materialIndex < static_cast<int>(scene.materials.size()) &&
               scene.materials[mesh.materialIndex].descriptorSet != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        pipelineLayout, 1, 1,
                                        &scene.materials[mesh.materialIndex].descriptorSet,
                                        0, nullptr);
            }

            ModelPushConstant push;
            push.model = mesh.transform.getMatrix();
            vkCmdPushConstants(cmd, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(ModelPushConstant), &push);

            VkBuffer vb[] = {mesh.vertexBuffer.buffer};
            VkDeviceSize offs[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vb, offs);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
    }

} // namespace vpbr