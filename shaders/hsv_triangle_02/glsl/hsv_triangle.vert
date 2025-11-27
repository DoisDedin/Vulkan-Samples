#version 450

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    vec3 color;
} ubo;

layout(location = 0) in vec2 inPos;

layout(location = 0) out vec3 vColor;

void main()
{
    vColor = ubo.color;
    gl_Position = ubo.mvp * vec4(inPos, 0.0, 1.0);
}
