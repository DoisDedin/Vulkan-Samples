#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in float inTriangle;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out float fragIntensity;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    vec4 offsets[3];
    vec4 params; // x: local rotation, y: scale, z: orbit radius, w: animate flag
} ubo;

const float kTau   = 6.28318530718;
const float kStep  = kTau / 3.0;

vec2 rotate(vec2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

void main()
{
    int triIndex = clamp(int(inTriangle + 0.5), 0, 2);
    float angle = ubo.params.x + float(triIndex) * kStep;

    vec2 local = rotate(inPos * ubo.params.y, angle);
    vec2 offset = ubo.offsets[triIndex].xy;
    vec2 finalPos = local + offset;

    gl_Position = ubo.mvp * vec4(finalPos, 0.0, 1.0);

    fragColor = inColor;
    fragIntensity = 0.6 + 0.4 * sin(angle);
}
