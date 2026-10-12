// src/renderer/mesh.h
#pragma once
#include "common/types.h"
#include "memory/buffer.h"
#include "renderer/vulkancontext.h"
#include <vector>

namespace vpbr {

    class MeshLoader {
    public:
        // Create a cube mesh
        static std::vector<Vertex> createCubeVertices();
        static std::vector<uint32_t> createCubeIndices();

        // Create a plane mesh
        static std::vector<Vertex> createPlaneVertices(float size);
        static std::vector<uint32_t> createPlaneIndices();

        // Create a sphere mesh
        static std::vector<Vertex> createSphereVertices(float radius, uint32_t segments);
        static std::vector<uint32_t> createSphereIndices(uint32_t segments);

        // Upload to GPU
        static void uploadMesh(VulkanContext& ctx, Buffer& vertexBuffer,
                               Buffer& indexBuffer,
                               const std::vector<Vertex>& vertices,
                               const std::vector<uint32_t>& indices);
    };

} // namespace vpbr