#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in float fragIntensity;

layout(location = 0) out vec4 outColor;

void main()
{
    float intensity = clamp(fragIntensity, 0.0, 1.0);
    outColor = vec4(fragColor * intensity, 1.0);
}
