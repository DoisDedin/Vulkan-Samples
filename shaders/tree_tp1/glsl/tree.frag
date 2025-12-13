#version 450

layout(location = 0) in float vRadius;
layout(location = 1) in vec3 vLocalPos;
layout(location = 2) flat in vec3 vSegmentA;
layout(location = 3) flat in vec3 vSegmentB;
layout(location = 4) flat in float vCapsuleRadius;
layout(location = 5) flat in float vTipFactor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 mvp;
    vec4 radiusRange; // x: min radius, y: max radius, z: range, w: unused
} ubo;

vec3 palette(float t)
{
    vec3 cold = vec3(0.0, 0.45, 0.95);
    vec3 mid  = vec3(0.45, 0.95, 0.55);
    vec3 hot  = vec3(1.0, 0.25, 0.15);

    if (t < 0.5)
    {
        return mix(cold, mid, t * 2.0);
    }
    else
    {
        return mix(mid, hot, (t - 0.5) * 2.0);
    }
}

vec3 closest_point_on_segment(vec3 p, vec3 a, vec3 b)
{
    vec3 ba = b - a;
    float len2 = dot(ba, ba);
    float h = 0.0;
    if (len2 > 1e-8)
    {
        h = clamp(dot(p - a, ba) / len2, 0.0, 1.0);
    }
    return a + ba * h;
}

float capsule_alpha(float distance, float radius, float solidity)
{
    float softness = mix(0.22, 0.08, solidity);
    float feather = max(radius * softness, 0.0015);
    return 1.0 - smoothstep(max(radius - feather, 0.0), radius, distance);
}

void main()
{
    float capsule_radius = max(vCapsuleRadius, 1e-4);
    vec3  closest        = closest_point_on_segment(vLocalPos, vSegmentA, vSegmentB);
    vec3  radial_vec     = vLocalPos - closest;
    float radial_len     = length(radial_vec);

    float solidity       = clamp(capsule_radius * 450.0, 0.0, 1.0);
    float alpha          = capsule_alpha(radial_len, capsule_radius, solidity);
    if (alpha <= 0.0)
    {
        discard;
    }

    float range      = max(ubo.radiusRange.z, 1e-6);
    float normalized = clamp((vRadius - ubo.radiusRange.x) / range, 0.0, 1.0);
    vec3  base_color = palette(normalized);

    float radial_ratio = clamp(radial_len / capsule_radius, 0.0, 1.0);

    vec3 normal = vec3(0.0, 0.0, 1.0);
    if (radial_len > 1e-6)
    {
        normal = normalize(radial_vec);
    }

    vec3 light_dir = normalize(vec3(-0.35, 0.45, 0.8));
    float lambert = clamp(dot(normal, light_dir), 0.2, 1.0);
    float highlight = pow(clamp(dot(normal, vec3(0.0, 0.0, 1.0)), 0.0, 1.0), 16.0);

    float center_emphasis = pow(1.0 - radial_ratio, 0.6);

    vec3 color = base_color * lambert;
    float tip_enhance = mix(1.0, 0.75, vTipFactor);
    color = mix(color * 0.5, color * 1.3, center_emphasis * tip_enhance);
    color += vec3(1.0) * highlight * 0.32;

    float edge_mix = smoothstep(1.0, 0.6, radial_ratio);
    vec3 rim_color = mix(vec3(0.05, 0.07, 0.1), base_color * 0.8, 0.5);
    color = mix(rim_color, color, edge_mix);

    color = mix(color * 0.9, color * 1.1, normalized);
    color = clamp(color, 0.0, 1.0);

    outColor = vec4(color, alpha);
}
