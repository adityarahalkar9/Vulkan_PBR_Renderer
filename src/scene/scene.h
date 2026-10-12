// src/scene/scene.h
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>

#include "common/types.h"
#include "memory/buffer.h"
#include "memory/image.h"
#include "transform.h"

namespace vpbr {

    struct Material {
        Image baseColorMap;
        Image metallicRoughnessMap;
        Image normalMap;
        Image occlusionMap;
        Image emissiveMap;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        PBRMaterialParams params;
        bool hasBaseColor = false;
        bool hasMetallicRoughness = false;
        bool hasNormal = false;
        bool hasOcclusion = false;
        bool hasEmissive = false;
    };

    struct Mesh {
        Buffer vertexBuffer;
        Buffer indexBuffer;
        uint32_t indexCount = 0;
        Transform transform;
        int materialIndex = -1;
        std::string name;
    };

    struct Camera {
        glm::vec3 position = glm::vec3(0.0f, 1.5f, 5.0f);
        glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        float fov = 60.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
    };

    struct Light {
        glm::vec4 position;  // w=0 for directional, w=1 for point
        glm::vec4 direction;
        glm::vec4 color;     // w = intensity
        glm::mat4 lightSpaceMatrix;
    };

    class Scene {
    public:
        void loadDefaultScene();
        void loadGltf(const std::string& path);
        void loadPrimitive(); // Load a hardcoded primitive for testing
        void cleanup(VulkanContext& ctx);

        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Light> lights;
        Camera camera;

    private:
        void createPrimitiveMesh(VulkanContext& ctx);
        void createDefaultMaterial(VulkanContext& ctx);
    };

}