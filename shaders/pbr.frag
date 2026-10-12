#version 460

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragTangent;
layout(location = 4) in vec4 fragLightSpacePos;

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

layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D occlusionMap;
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

layout(set = 1, binding = 5) uniform MaterialUBO {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    vec4 emissiveFactor;
} material;

layout(set = 2, binding = 0) uniform sampler2D shadowMap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float shadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Vulkan NDC xy [-1, 1] to [0, 1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Check outside shadow frustum bounds
    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main() {
    // 1. Texture sampling and material factor application
    vec4 baseColor = texture(baseColorMap, fragTexCoord) * material.baseColorFactor;
    vec3 albedo = baseColor.rgb;

    vec4 mrSample = texture(metallicRoughnessMap, fragTexCoord);
    float metallic = clamp(mrSample.b * material.metallicFactor, 0.0, 1.0);
    float roughness = clamp(mrSample.g * material.roughnessFactor, 0.04, 1.0);

    float rawAo = texture(occlusionMap, fragTexCoord).r;
    float ao = mix(1.0, rawAo, material.occlusionStrength);

    vec3 emissive = texture(emissiveMap, fragTexCoord).rgb * material.emissiveFactor.rgb;

    // 2. Reconstruct TBN matrix
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(fragTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * fragTangent.w;
    mat3 TBN = mat3(T, B, N);

    // 3. Normal mapping
    vec3 normalSample = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    normalSample.xy *= material.normalScale;
    N = normalize(TBN * normalSample);

    vec3 V = normalize(camera.cameraPos.xyz - fragWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // 4. Evaluate Cook-Torrance PBR for each active light
    int count = clamp(lightsUBO.numLights, 0, 4);
    for (int i = 0; i < count; ++i) {
        vec3 L;
        vec3 radiance;

        // Position.w == 0.0 implies directional light; otherwise point light
        if (lightsUBO.lights[i].position.w == 0.0 || length(lightsUBO.lights[i].direction.xyz) > 0.001) {
            // Directional light
            L = normalize(-lightsUBO.lights[i].direction.xyz);
            radiance = lightsUBO.lights[i].color.rgb * lightsUBO.lights[i].color.a;
        } else {
            // Point light
            vec3 lightToPos = lightsUBO.lights[i].position.xyz - fragWorldPos;
            float distance = length(lightToPos);
            L = normalize(lightToPos);
            float attenuation = 1.0 / max(distance * distance, 0.0001);
            radiance = lightsUBO.lights[i].color.rgb * lightsUBO.lights[i].color.a * attenuation;
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0);

        if (NdotL > 0.0) {
            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * NdotV * NdotL + 0.0001;
            vec3 specular = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            float shadow = 0.0;
            if (i == 0) {
                shadow = shadowCalculation(fragLightSpacePos, N, L);
            }

            Lo += (1.0 - shadow) * (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

    // 6. Ambient term
    vec3 ambient = vec3(0.03) * albedo * ao;

    // 7. Combined with emissive
    vec3 color = ambient + Lo + emissive;

    // 8. Reinhard tonemapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, baseColor.a);
}
