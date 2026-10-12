#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>

#include "common/types.h"
#include "memory/buffer.h"
#include "memory/image.h"
#include "scene/transform.h"
#include "renderer/vulkancontext.h"

namespace vpbr {

    struct Material {
        Image baseColorMap;
        Image metallicRoughnessMap;
        Image normalMap;
        Image occlusionMap;
        Image emissiveMap;

        bool hasBaseColor = false;
        bool hasMetallicRoughness = false;
        bool hasNormal = false;
        bool hasOcclusion = false;
        bool hasEmissive = false;

        PBRMaterialParams params;
        Buffer paramsUBO;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        std::string name;
    };

    struct Mesh {
        Buffer vertexBuffer;
        Buffer indexBuffer;
        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
        Transform transform;
        int materialIndex = -1;
        std::string name;

        void destroy(VulkanContext& ctx) {
            vertexBuffer.destroy(ctx.allocator);
            indexBuffer.destroy(ctx.allocator);
        }
    };

    struct Camera {
        glm::vec3 position = glm::vec3(0.0f, 1.8f, 5.0f);
        glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

        float yaw = -90.0f;
        float pitch = 0.0f;
        float movementSpeed = 3.0f;
        float mouseSensitivity = 0.1f;

        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;

        glm::mat4 getViewMatrix() const {
            return glm::lookAt(position, position + front, up);
        }

        glm::mat4 getProjectionMatrix(float aspect) const {
            glm::mat4 proj = glm::perspective(
                glm::radians(fov), aspect, nearPlane, farPlane);
            proj[1][1] *= -1.0f;
            return proj;
        }

        void updateCameraVectors() {
            glm::vec3 f;
            f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            f.y = sin(glm::radians(pitch));
            f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            front = glm::normalize(f);
            right = glm::normalize(glm::cross(front, worldUp));
            up = glm::normalize(glm::cross(right, front));
        }
    };

    struct Light {
        glm::vec4 position;
        glm::vec4 direction;
        glm::vec4 color;
        glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);
    };

    class Scene {
    public:
        void loadDefaultScene(VulkanContext& ctx);
        void cleanup(VulkanContext& ctx);

        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Light> lights;
        Camera camera;

        glm::mat4 computeLightSpaceMatrix(uint32_t lightIndex) const;
    };

} // namespace vpbr