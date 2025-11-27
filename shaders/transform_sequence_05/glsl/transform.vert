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

layout(location = 0) out vec4 vColor;

void main()
{
    gl_Position = ubo.vp * push_data.model * vec4(inPos, 1.0);
    vColor = push_data.color;
}
