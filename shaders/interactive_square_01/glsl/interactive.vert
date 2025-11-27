#version 450

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
} ubo;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vColor;

void main()
{
    vColor = inColor;
    gl_Position = ubo.mvp * vec4(inPos, 0.0, 1.0);
}
