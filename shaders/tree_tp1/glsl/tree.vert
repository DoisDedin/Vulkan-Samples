#version 450

// Vertex shader responsável por aplicar a matriz MVP e encaminhar para o fragment shader
// todas as informações necessárias para o SDF (extremos da cápsula, raio, fator da ponta).

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inCapsuleStart;
layout(location = 2) in vec3 inCapsuleEnd;
layout(location = 3) in float inCapsuleRadius;
layout(location = 4) in float inDatasetRadius;
layout(location = 5) in float inDepthFactor;

layout(location = 0) out float vDatasetRadius;
layout(location = 1) out vec3 vLocalPos;
layout(location = 2) flat out vec3 vCapsuleStart;
layout(location = 3) flat out vec3 vCapsuleEnd;
layout(location = 4) flat out float vCapsuleRadius;
layout(location = 5) flat out float vDepthFactor;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    vec4 radiusRange; // x: min radius, y: max radius, z: range, w: unused
} ubo;

void main()
{
    vDatasetRadius = inDatasetRadius;
    vLocalPos = inPosition;
    vCapsuleStart = inCapsuleStart;
    vCapsuleEnd = inCapsuleEnd;
    vCapsuleRadius = inCapsuleRadius;
    vDepthFactor = inDepthFactor;
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
