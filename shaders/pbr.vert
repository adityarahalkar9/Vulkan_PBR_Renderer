#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} camera;

struct LightData {
    vec4 position;
    vec4 direction;
    vec4 color;
    mat4 lightSpaceMatrix;
};

layout(set = 0, binding = 1) uniform LightsUBO {
    LightData lights[4];
    int numLights;
} lightsUBO;

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragTangent;
layout(location = 4) out vec4 fragLightSpacePos;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = camera.viewProj * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    fragTangent = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);

    fragWorldPos = worldPos.xyz;
    fragTexCoord = inTexCoord;
    fragLightSpacePos = lightsUBO.lights[0].lightSpaceMatrix * worldPos;
}
