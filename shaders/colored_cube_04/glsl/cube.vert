#version 450

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    mat4 model;
    mat3 normalMatrix;
    vec3 lightDir;
    float pad;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec3 vNormal;

void main()
{
    gl_Position = ubo.mvp * vec4(inPos, 1.0);
    vColor = inColor;
    vNormal = normalize(ubo.normalMatrix * inNormal);
}
