#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace vpbr {

    struct Transform {
        glm::vec3 position = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale = glm::vec3(1.0f);

        [[nodiscard]] glm::mat4 getMatrix() const {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 R = glm::toMat4(rotation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            return T * R * S;
        }
    };

}
