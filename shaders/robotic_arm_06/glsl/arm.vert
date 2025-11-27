#version 450

layout(set = 0, binding = 0) uniform UBO
{
    mat4 vp;
} ubo;

layout(push_constant) uniform PushData
{
    mat4 model;
    vec4 color;
} push_data;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec3 vNormal;

void main()
{
    vec4 worldPos = push_data.model * vec4(inPos, 1.0);
    gl_Position = ubo.vp * worldPos;
    vColor = push_data.color;
    vNormal = mat3(push_data.model) * inNormal;
}
