#pragma once

#include <vector>
#include <utility>
#include <cstdint>

#include "common/types.h"
#include "memory/buffer.h"
#include "scene/scene.h"
#include "renderer/vulkancontext.h"

namespace vpbr {

    class MeshLoader {
    public:
        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
            createCube(float size = 1.0f);

        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
            createPlane(float size = 1.0f);

        static std::pair<std::vector<Vertex>, std::vector<uint32_t>>
            createSphere(float radius = 0.5f, uint32_t segments = 32);

        static void uploadMesh(VulkanContext& ctx, Mesh& mesh,
                               const std::vector<Vertex>& vertices,
                               const std::vector<uint32_t>& indices);
    };

} // namespace vpbr