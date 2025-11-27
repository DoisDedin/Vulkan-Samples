#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(-0.3, -1.0, -0.2));
    float diffuse = max(dot(N, -L), 0.0);
    vec3 ambient = 0.2 * vColor.rgb;
    vec3 lighting = ambient + vColor.rgb * (0.8 * diffuse);
    outColor = vec4(lighting, vColor.a);
}
