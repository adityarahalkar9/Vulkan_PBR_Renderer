// src/scene/scene.cpp
#include "scene/scene.h"
#include "renderer/vulkancontext.h"
#include "utils/logger.h"
#include "utils/vulkanutils.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vpbr {

    void Scene::loadDefaultScene() {
        // Create a simple cube + plane + light setup for initial testing
        LOG_INFO("Loading default scene (cube + directional light)...");

        // Add a directional light
        Light dirLight;
        dirLight.position = glm::vec4(0.0f, 10.0f, 5.0f, 0.0f); // directional
        dirLight.direction = glm::vec4(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f)), 0.0f);
        dirLight.color = glm::vec4(1.0f, 1.0f, 0.95f, 3.0f);

        // Compute light space matrix for shadow mapping
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 20.0f);
        glm::vec3 lightPos = -glm::vec3(dirLight.direction) * 15.0f;
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f),
                                          glm::vec3(0.0f, 1.0f, 0.0f));
        dirLight.lightSpaceMatrix = lightProjection * lightView;
        lights.push_back(dirLight);

        // Add a fill point light
        Light pointLight;
        pointLight.position = glm::vec4(2.0f, 2.0f, 2.0f, 1.0f); // point
        pointLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        pointLight.color = glm::vec4(0.3f, 0.3f, 0.5f, 5.0f);
        pointLight.lightSpaceMatrix = glm::mat4(1.0f);
        lights.push_back(pointLight);
    }

    void Scene::cleanup(VulkanContext& ctx) {
        for(auto& mesh : meshes) {
            mesh.vertexBuffer.destroy(ctx.allocator);
            mesh.indexBuffer.destroy(ctx.allocator);
        }
        meshes.clear();

        for(auto& mat : materials) {
            if(mat.hasBaseColor) mat.baseColorMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasMetallicRoughness) mat.metallicRoughnessMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasNormal) mat.normalMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasOcclusion) mat.occlusionMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasEmissive) mat.emissiveMap.destroy(ctx.allocator, ctx.device);
        }
        materials.clear();
        lights.clear();
    }

}