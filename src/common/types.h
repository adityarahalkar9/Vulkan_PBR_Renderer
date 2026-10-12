#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>

namespace vpbr {

    inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    inline constexpr uint32_t SHADOW_MAP_SIZE = 2048;
    inline constexpr uint32_t MAX_LIGHTS = 4;

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec4 tangent;

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription d{};
            d.binding = 0;
            d.stride = sizeof(Vertex);
            d.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return d;
        }

        static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 4> d{};
            d[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex, position)};
            d[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex, normal)};
            d[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(Vertex, texCoord)};
            d[3] = {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)};
            return d;
        }
    };

    struct CameraUBO {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 viewProj;
        alignas(16) glm::vec4 cameraPos;
    };

    struct LightUBOData {
        alignas(16) glm::vec4 position;
        alignas(16) glm::vec4 direction;
        alignas(16) glm::vec4 color;
        alignas(16) glm::mat4 lightSpaceMatrix;
    };

    struct LightsUBO {
        LightUBOData lights[MAX_LIGHTS];
        alignas(4) int   numLights;
        alignas(4) float padding[3];
    };

    struct PBRMaterialParams {
        alignas(16) glm::vec4 baseColorFactor = glm::vec4(1.0f);
        alignas(4)  float metallicFactor = 1.0f;
        alignas(4)  float roughnessFactor = 1.0f;
        alignas(4)  float normalScale = 1.0f;
        alignas(4)  float occlusionStrength = 1.0f;
        alignas(16) glm::vec4 emissiveFactor = glm::vec4(0.0f);
    };

    struct ModelPushConstant {
        glm::mat4 model;
    };

} // namespace vpbr