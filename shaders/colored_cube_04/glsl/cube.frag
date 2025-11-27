#version 450

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    mat4 model;
    mat3 normalMatrix;
    vec3 lightDir;
    float pad;
} ubo_f;

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    float diffuse = max(dot(normalize(-ubo_f.lightDir), normalize(vNormal)), 0.0);
    vec3 litColor = vColor * (0.3 + 0.7 * diffuse);
    outColor = vec4(litColor, 1.0);
}
