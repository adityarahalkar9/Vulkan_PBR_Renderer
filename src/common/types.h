#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>

namespace vpbr {

    // ── Engine Constants ──────────────────────────────────────────────
    inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    inline constexpr uint32_t SHADOW_MAP_SIZE = 2048;
    inline constexpr uint32_t MAX_LIGHTS = 4;
    inline constexpr uint32_t PROFILER_HISTORY_SIZE = 120;

    // ── Vertex ────────────────────────────────────────────────────────
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec4 tangent;

        static VkVertexInputBindingDescription getBindingDescription() {
            return {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        }

        static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
            return {{
                { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex, position) },
                { 1, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex, normal)   },
                { 2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(Vertex, texCoord) },
                { 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)  },
            }};
        }
    };

    // ── GPU Uniform Structs (must match GLSL layouts exactly) ─────────

    struct CameraUBO {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 viewProj;
        alignas(16) glm::vec4 cameraPos; // xyz = position, w = unused
    };

    struct LightUBOData {
        alignas(16) glm::vec4 position;          // xyz = pos/dir, w = type (0=dir, 1=point)
        alignas(16) glm::vec4 direction;         // xyz = direction, w = unused
        alignas(16) glm::vec4 color;             // xyz = color, w = intensity
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
