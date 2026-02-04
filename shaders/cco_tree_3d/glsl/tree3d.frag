#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in float vDatasetRadius;
layout(location = 3) in float vDepthFactor;
layout(location = 4) in vec3 vGouraudColor;
layout(location = 5) flat in float vSegmentId;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 mvp;
    mat4 normalMatrix;
    vec4 lightDir;
    vec4 cameraPos;
    vec4 radiusRange;
    vec4 params; // x: lighting model (0 Gouraud, 1 Phong), y: selected id
} ubo;

vec3 palette(float t)
{
    vec3 cold = vec3(0.05, 0.4, 0.95);
    vec3 mid  = vec3(0.35, 0.9, 0.6);
    vec3 hot  = vec3(0.95, 0.25, 0.15);

    if (t < 0.5)
    {
        return mix(cold, mid, t * 2.0);
    }
    else
    {
        return mix(mid, hot, (t - 0.5) * 2.0);
    }
}

vec3 phong_lighting(vec3 baseColor, vec3 normal, vec3 worldPos)
{
    vec3 lightDir = normalize(ubo.lightDir.xyz);
    vec3 viewDir = normalize(ubo.cameraPos.xyz - worldPos);

    float lambert = max(dot(normal, lightDir), 0.12);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(reflectDir, viewDir), 0.0), 32.0);

    float depthTone = mix(0.7, 1.0, vDepthFactor);
    return baseColor * lambert * depthTone + vec3(1.0) * spec * 0.3;
}

void main()
{
    float range = max(ubo.radiusRange.z, 1e-6);
    float normalized = clamp((vDatasetRadius - ubo.radiusRange.x) / range, 0.0, 1.0);
    vec3 baseColor = palette(normalized);

    vec3 color = vGouraudColor;
    if (ubo.params.x > 0.5)
    {
        color = phong_lighting(baseColor, normalize(vNormal), vWorldPos);
    }

    if (abs(vSegmentId - ubo.params.y) < 0.5)
    {
        color = mix(color, vec3(1.0, 0.9, 0.35), 0.55);
    }

    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
