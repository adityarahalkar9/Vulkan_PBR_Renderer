#version 460

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform LightSpaceMatrix {
    mat4 lightSpaceMatrix;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

void main() {
    gl_Position = ubo.lightSpaceMatrix * push.model * vec4(inPosition, 1.0);
}
