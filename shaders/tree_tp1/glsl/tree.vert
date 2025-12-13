#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inSegmentA;
layout(location = 2) in vec3 inSegmentB;
layout(location = 3) in float inCapsuleRadius;
layout(location = 4) in float inRadiusValue;
layout(location = 5) in float inTipFactor;

layout(location = 0) out float vRadius;
layout(location = 1) out vec3 vLocalPos;
layout(location = 2) flat out vec3 vSegmentA;
layout(location = 3) flat out vec3 vSegmentB;
layout(location = 4) flat out float vCapsuleRadius;
layout(location = 5) flat out float vTipFactor;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    vec4 radiusRange; // x: min radius, y: max radius, z: range, w: unused
} ubo;

void main()
{
    vRadius = inRadiusValue;
    vLocalPos = inPosition;
    vSegmentA = inSegmentA;
    vSegmentB = inSegmentB;
    vCapsuleRadius = inCapsuleRadius;
    vTipFactor = inTipFactor;
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
