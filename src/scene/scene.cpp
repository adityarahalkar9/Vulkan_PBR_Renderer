#include "scene/scene.h"
#include "renderer/mesh_loader.h"
#include "utils/logger.h"

#include <glm/gtc/matrix_transform.hpp>

namespace vpbr {

    void Scene::loadDefaultScene(VulkanContext& ctx) {
        LOG_INFO("Loading default scene...");

        // Material 0: grey floor
        {
            Material m;
            m.name = "grey";
            m.params.baseColorFactor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
            m.params.metallicFactor = 0.05f;
            m.params.roughnessFactor = 0.8f;
            materials.push_back(std::move(m));
        }

        // Material 1: red plastic
        {
            Material m;
            m.name = "red";
            m.params.baseColorFactor = glm::vec4(0.85f, 0.15f, 0.1f, 1.0f);
            m.params.metallicFactor = 0.0f;
            m.params.roughnessFactor = 0.35f;
            materials.push_back(std::move(m));
        }

        // Material 2: gold metal
        {
            Material m;
            m.name = "gold";
            m.params.baseColorFactor = glm::vec4(1.0f, 0.766f, 0.336f, 1.0f);
            m.params.metallicFactor = 1.0f;
            m.params.roughnessFactor = 0.25f;
            materials.push_back(std::move(m));
        }

        // Ground plane
        {
            auto [verts, indices] = MeshLoader::createPlane(30.0f);
            Mesh mesh;
            MeshLoader::uploadMesh(ctx, mesh, verts, indices);
            mesh.name = "ground";
            mesh.materialIndex = 0;
            mesh.transform.position = glm::vec3(0.0f);
            meshes.push_back(std::move(mesh));
        }

        // Cube (red)
        {
            auto [verts, indices] = MeshLoader::createCube(1.0f);
            Mesh mesh;
            MeshLoader::uploadMesh(ctx, mesh, verts, indices);
            mesh.name = "cube";
            mesh.materialIndex = 1;
            mesh.transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
            meshes.push_back(std::move(mesh));
        }

        // Sphere (gold)
        {
            auto [verts, indices] = MeshLoader::createSphere(0.6f, 32);
            Mesh mesh;
            MeshLoader::uploadMesh(ctx, mesh, verts, indices);
            mesh.name = "sphere";
            mesh.materialIndex = 2;
            mesh.transform.position = glm::vec3(2.5f, 0.6f, 0.0f);
            meshes.push_back(std::move(mesh));
        }

        // Rotated cube
        {
            auto [verts, indices] = MeshLoader::createCube(0.8f);
            Mesh mesh;
            MeshLoader::uploadMesh(ctx, mesh, verts, indices);
            mesh.name = "cube2";
            mesh.materialIndex = 0;
            mesh.transform.position = glm::vec3(-2.5f, 0.4f, 1.0f);
            mesh.transform.rotation = glm::angleAxis(
                glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            meshes.push_back(std::move(mesh));
        }

        // Directional light (sun)
        {
            Light sun;
            sun.position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            sun.direction = glm::vec4(
                glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)), 0.0f);
            sun.color = glm::vec4(1.0f, 0.98f, 0.95f, 2.5f);
            sun.lightSpaceMatrix = computeLightSpaceMatrix(0);
            lights.push_back(sun);
        }

        // Point light (blue)
        {
            Light pl;
            pl.position = glm::vec4(-3.0f, 3.0f, 2.0f, 1.0f);
            pl.direction = glm::vec4(0.0f);
            pl.color = glm::vec4(0.2f, 0.3f, 0.8f, 4.0f);
            lights.push_back(pl);
        }

        // Point light (warm)
        {
            Light pl;
            pl.position = glm::vec4(3.0f, 2.0f, -2.0f, 1.0f);
            pl.direction = glm::vec4(0.0f);
            pl.color = glm::vec4(1.0f, 0.8f, 0.3f, 3.0f);
            lights.push_back(pl);
        }

        LOG_INFO("Scene: %zu meshes, %zu materials, %zu lights",
                 meshes.size(), materials.size(), lights.size());
    }

    void Scene::cleanup(VulkanContext& ctx) {
        for(auto& mesh : meshes) {
            mesh.destroy(ctx);
        }
        meshes.clear();

        for(auto& mat : materials) {
            if(mat.paramsUBO.buffer != VK_NULL_HANDLE) {
                mat.paramsUBO.destroy(ctx.allocator);
            }
            if(mat.hasBaseColor) mat.baseColorMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasMetallicRoughness) mat.metallicRoughnessMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasNormal) mat.normalMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasOcclusion) mat.occlusionMap.destroy(ctx.allocator, ctx.device);
            if(mat.hasEmissive) mat.emissiveMap.destroy(ctx.allocator, ctx.device);
        }
        materials.clear();
        lights.clear();
        LOG_INFO("Scene cleaned up.");
    }

    glm::mat4 Scene::computeLightSpaceMatrix(uint32_t lightIndex) const {
        if(lightIndex >= lights.size()) return glm::mat4(1.0f);

        const Light& light = lights[lightIndex];

        if(light.position.w == 0.0f) {
            glm::vec3 dir = glm::normalize(glm::vec3(light.direction));
            glm::vec3 pos = -dir * 15.0f;
            glm::mat4 view = glm::lookAt(pos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
            glm::mat4 proj = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 1.0f, 40.0f);
            proj[1][1] *= -1.0f;
            return proj * view;
        }
        else {
            glm::vec3 pos = glm::vec3(light.position);
            glm::mat4 view = glm::lookAt(pos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
            glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
            proj[1][1] *= -1.0f;
            return proj * view;
        }
    }

} // namespace vpbr