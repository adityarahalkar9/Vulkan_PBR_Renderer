#include "renderer/mesh_loader.h"
#include "utils/logger.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vpbr {

    std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        MeshLoader::createCube(float size) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        float h = size * 0.5f;

        auto addFace = [&](const glm::vec3& normal,
                           const glm::vec3& up,
                           const glm::vec3& right) {
                               uint32_t base = static_cast<uint32_t>(vertices.size());
                               glm::vec3 tangent = glm::normalize(right);

                               std::array<glm::vec3, 4> corners = {
                                   -right * h - up * h,
                                    right * h - up * h,
                                    right * h + up * h,
                                   -right * h + up * h
                               };

                               std::array<glm::vec2, 4> uvs = {
                                    glm::vec2(0.0f, 0.0f),
                                    glm::vec2(1.0f, 0.0f),
                                    glm::vec2(1.0f, 1.0f),
                                    glm::vec2(0.0f, 1.0f)
                               };

                               for(int i = 0; i < 4; i++) {
                                   Vertex v;
                                   v.position = corners[i] + normal * h;
                                   v.normal = normal;
                                   v.texCoord = uvs[i];
                                   v.tangent = glm::vec4(tangent, 1.0f);
                                   vertices.push_back(v);
                               }

                               indices.insert(indices.end(), {
                                   base, base + 1, base + 2,
                                   base, base + 2, base + 3
                                              });
            };

        glm::vec3 n(0.0f);
        addFace(glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec3(1, 0, 0));
        addFace(glm::vec3(0, 0, -1), glm::vec3(0, 1, 0), glm::vec3(-1, 0, 0));
        addFace(glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, -1));
        addFace(glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
        addFace(glm::vec3(0, 1, 0), glm::vec3(0, 0, -1), glm::vec3(1, 0, 0));
        addFace(glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0));

        (void)n;
        return {vertices, indices};
    }

    std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        MeshLoader::createPlane(float size) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        float h = size * 0.5f;

        Vertex v;
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

        v.position = glm::vec3(-h, 0.0f, -h); v.texCoord = glm::vec2(0, 0); vertices.push_back(v);
        v.position = glm::vec3(h, 0.0f, -h); v.texCoord = glm::vec2(1, 0); vertices.push_back(v);
        v.position = glm::vec3(h, 0.0f, h); v.texCoord = glm::vec2(1, 1); vertices.push_back(v);
        v.position = glm::vec3(-h, 0.0f, h); v.texCoord = glm::vec2(0, 1); vertices.push_back(v);

        indices = {0, 1, 2, 0, 2, 3};
        return {vertices, indices};
    }

    std::pair<std::vector<Vertex>, std::vector<uint32_t>>
        MeshLoader::createSphere(float radius, uint32_t segments) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        const uint32_t latSegs = segments;
        const uint32_t lonSegs = segments;

        for(uint32_t lat = 0; lat <= latSegs; lat++) {
            float theta = static_cast<float>(lat) / latSegs * static_cast<float>(M_PI);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for(uint32_t lon = 0; lon <= lonSegs; lon++) {
                float phi = static_cast<float>(lon) / lonSegs * 2.0f * static_cast<float>(M_PI);

                Vertex v;
                v.position = glm::vec3(
                    radius * sinTheta * std::cos(phi),
                    radius * cosTheta,
                    radius * sinTheta * std::sin(phi));
                v.normal = glm::normalize(v.position);
                v.texCoord = glm::vec2(
                    static_cast<float>(lon) / lonSegs,
                    1.0f - static_cast<float>(lat) / latSegs);

                glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), v.normal));
                v.tangent = glm::vec4(tangent, 1.0f);
                vertices.push_back(v);
            }
        }

        for(uint32_t lat = 0; lat < latSegs; lat++) {
            for(uint32_t lon = 0; lon < lonSegs; lon++) {
                uint32_t first = (lat * (lonSegs + 1)) + lon;
                uint32_t second = first + lonSegs + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        return {vertices, indices};
    }

    void MeshLoader::uploadMesh(VulkanContext& ctx, Mesh& mesh,
                                const std::vector<Vertex>& vertices,
                                const std::vector<uint32_t>& indices)
    {
        mesh.vertexCount = static_cast<uint32_t>(vertices.size());
        mesh.indexCount = static_cast<uint32_t>(indices.size());

        VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
        mesh.vertexBuffer.create(ctx.allocator, vertexSize,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VMA_MEMORY_USAGE_GPU_ONLY);

        {
            Buffer staging;
            staging.create(ctx.allocator, vertexSize,
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VMA_MEMORY_USAGE_CPU_ONLY);
            staging.upload(ctx.allocator, vertices.data(), vertexSize);

            VkCommandBuffer cmd = ctx.beginSingleTimeCommands();
            VkBufferCopy region{};
            region.size = vertexSize;
            vkCmdCopyBuffer(cmd, staging.buffer, mesh.vertexBuffer.buffer, 1, &region);
            ctx.endSingleTimeCommands(cmd);
            staging.destroy(ctx.allocator);
        }

        VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();
        mesh.indexBuffer.create(ctx.allocator, indexSize,
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VMA_MEMORY_USAGE_GPU_ONLY);

        {
            Buffer staging;
            staging.create(ctx.allocator, indexSize,
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VMA_MEMORY_USAGE_CPU_ONLY);
            staging.upload(ctx.allocator, indices.data(), indexSize);

            VkCommandBuffer cmd = ctx.beginSingleTimeCommands();
            VkBufferCopy region{};
            region.size = indexSize;
            vkCmdCopyBuffer(cmd, staging.buffer, mesh.indexBuffer.buffer, 1, &region);
            ctx.endSingleTimeCommands(cmd);
            staging.destroy(ctx.allocator);
        }
    }

} // namespace vpbr