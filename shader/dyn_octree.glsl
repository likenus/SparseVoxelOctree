#ifndef OCTREE_GLSL
#define OCTREE_GLSL

#ifndef DYN_OCTREE_SET
#define DYN_OCTREE_SET 0
#endif // DYN_OCTREE_SET

layout(set = DYN_OCTREE_SET, binding = 0) readonly buffer uuDynOctree { vec3 uOctree[]; };

float scale = .5f;

bool intersectAABB(vec3 o, vec3 d, inout float t_min, inout float t_max) {
    vec3 p = uOctree[0].xyz;
    vec3 p_min = p - vec3(scale);
    vec3 p_max = p + vec3(scale);

    vec3 div = vec3(1.f) / d;
    vec3 t1 = (p_min - o) * div;
    vec3 t2 = (p_max - o) * div;

    vec3 t_min2 = min(t1, t2);
    vec3 t_max2 = max(t1, t2);

    t_min = max(max(t_min2.x, t_min2.y), max(t_min2.z, t_min));
    t_max = min(min(t_max2.x, t_max2.y), min(t_max2.z, t_max));

    return t_min <= t_max;
}

bool Simple_Raymarch(vec3 o, vec3 d, out vec3 o_pos, out vec4 o_color) {
    float t_min = -100000.f, t_max = 100000.f;
    bool hit = intersectAABB(o, d, t_min, t_max);

    if (hit) {
        o_pos = o + t_min * d;
        o_color = vec4(0.f, 1.f, 0.f, 1.f);
    } else {
        o_color = vec4(uOctree[0], 1.f);
    }

    return hit;
}

#endif // OCTREE_GLSL