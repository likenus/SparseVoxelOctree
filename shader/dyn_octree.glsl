#ifndef OCTREE_GLSL
#define OCTREE_GLSL

#ifndef DYN_OCTREE_SET
#define DYN_OCTREE_SET 0
#endif // DYN_OCTREE_SET

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38


layout(set = DYN_OCTREE_SET, binding = 0) uniform uuOctreeInfo { uint uSize, uRes; }; // uSize == uVoxels.size(), voxel scale = 2^(-uRes)
layout(set = DYN_OCTREE_SET, binding = 1) readonly buffer uuVoxelBuffer { vec4 uVoxels[]; };
layout(set = DYN_OCTREE_SET, binding = 2) readonly buffer uuOctreeBuffer { uint uOctree[]; };

bool IntersectVoxel(vec3 p, vec3 o, vec3 d, inout float t_min, inout float t_max);
bool IntersectVoxel(vec3 p_min, vec3 p_max, vec3 o, vec3 d, inout float t_min, inout float t_max);

uint root = 1;
uint octree_size = (1 << 20);

float voxel_scale = exp2(-float(uRes));

uint getChildPointer(uint descriptor) {
    return descriptor >> 17;
}

uint getValidMask(uint descriptor) {
    return descriptor & 0xff00;
}

uint getValidBit(uint descriptor, uint child_idx) {
    return descriptor & (0x10 << child_idx);
}

// bitmask in [0, 7]
vec3 BitmaskToVec3(uint bitmask) {
    uint x = bitmask & (1 << 2) >> 2, y = (bitmask & (1 << 1)) >> 1, z = (bitmask & (1 << 0));
    return vec3(x, y, z);
}

bool IntersectVoxel(vec3 p, vec3 o, vec3 d, inout float t_min, inout float t_max) {
    vec3 p_min = p - vec3(voxel_scale);
    vec3 p_max = p + vec3(voxel_scale);

    return IntersectVoxel(p_min, p_max, o, d, t_min, t_max);
}

bool IntersectVoxel(vec3 p_min, vec3 p_max, vec3 o, vec3 d, inout float t_min, inout float t_max) {

    vec3 div = vec3(1.f) / d;
    vec3 t1 = (p_min - o) * div;
    vec3 t2 = (p_max - o) * div;

    vec3 t_min2 = min(t1, t2);
    vec3 t_max2 = max(t1, t2);

    t_min = max(max(t_min2.x, t_min2.y), max(t_min2.z, t_min));
    t_max = min(min(t_max2.x, t_max2.y), min(t_max2.z, t_max));

    return t_min <= t_max && t_max > 0;
}

bool SimpleRaymarch(vec3 o, vec3 d, out vec3 o_pos, out vec4 o_color) {
    float t = FLT_MAX;
    bool hit = false;

    for (int i = 0; i < uSize; i++) {
        float t_min = -FLT_MAX, t_max = FLT_MAX;
        if (IntersectVoxel(uVoxels[i].xyz, o, d, t_min, t_max)) {
            t = min(t, t_min);
            hit = true;
        }
    }

    if (hit) {
        o_pos = o + t * d;
        o_color = vec4(0.f, 1.f, 0.f, 1.f);
    }

    return hit;
}

bool IntersectOctree(vec3 o, vec3, out vec3 o_pos, out vec3 o_col) {
    return false;
}

bool WireFrameVoxel(vec3 o, vec3 d, vec3 p_min, float octree_scale, out vec3 o_pos, out vec4 o_col) {
    float t = FLT_MAX;
    bool hit = false;

    vec3 p = p_min;
    float eps = 5e-4f * exp2(length(o - p));

    for (uint j = 0; j < 8; j++) {
        uint x = j & (1 << 0), y = (j & (1 << 1)) >> 1, z = (j & (1 << 2)) >> 2;
        vec3 p0 = p + (vec3(x, y, z) * 2 * octree_scale);

        if (x == 0) {
            vec3 p1 = p0 + vec3(2 * octree_scale, 0, 0);
            float t_min = -FLT_MAX, t_max = FLT_MAX;
            if (IntersectVoxel(p0 - eps, p1 + eps, o, d, t_min, t_max)) {
                t = min(t, t_min);
                hit = true;
            }
        }
        if (y == 0) {
            vec3 p1 = p0 + vec3(0, 2 * octree_scale, 0);
            float t_min = -FLT_MAX, t_max = FLT_MAX;
            if (IntersectVoxel(p0 - eps, p1 + eps, o, d, t_min, t_max)) {
                t = min(t, t_min);
                hit = true;
            }
        }
        if (z == 0) {
            vec3 p1 = p0 + vec3(0, 0, 2 * octree_scale);
            float t_min = -FLT_MAX, t_max = FLT_MAX;
            if (IntersectVoxel(p0 - eps, p1 + eps, o, d, t_min, t_max)) {
                t = min(t, t_min);
                hit = true;
            }
        }
    }

    if (hit) {
        o_pos = o + t * d;
        o_col = vec4(0.5f, 0.5f, 0.5f, 1.f);
    } else {
        o_col = vec4(1.f);
    }

    return hit;
}

bool RaycastOctree(vec3 o, vec3 d, out float hit_t, out vec3 hit_pos) {
    const uint s_max = 23;
    const float epsilon = exp2(-float(s_max));

    uvec2 stack[s_max + 1];


    if (abs(d.x) < epsilon) d.x = sign(d.x) * epsilon;
    if (abs(d.y) < epsilon) d.y = sign(d.y) * epsilon;
    if (abs(d.z) < epsilon) d.z = sign(d.z) * epsilon;

    vec3 t_coef = 1.f / -abs(d);
    vec3 t_bias = t_coef * o;

    uint octant_mask = 7u;
    if (d.x > 0.0f) octant_mask ^= 1u, t_bias.x = 3.0f * t_coef.x - t_bias.x;
    if (d.y > 0.0f) octant_mask ^= 2u, t_bias.y = 3.0f * t_coef.y - t_bias.y;
    if (d.z > 0.0f) octant_mask ^= 4u, t_bias.z = 3.0f * t_coef.z - t_bias.z;

    // Initialize the active span of t-values.
    float t_min = max(max(2.0f * t_coef.x - t_bias.x, 2.0f * t_coef.y - t_bias.y), 2.0f * t_coef.z - t_bias.z);
    float t_max = min(min(t_coef.x - t_bias.x, t_coef.y - t_bias.y), t_coef.z - t_bias.z);
    t_min = max(t_min, 0.0f);
    float h = t_max;

    uint parent = root;
    uint child_descriptor = 0u;
    uint idx = 0;
    vec3 pos = vec3(1.f);
    uint scale = s_max - 1;
    float scale_exp2 = 0.5f;


    if (1.5f * t_coef.x - t_bias.x > t_min) idx ^= 1u, pos.x = 1.5f;
    if (1.5f * t_coef.y - t_bias.y > t_min) idx ^= 2u, pos.y = 1.5f;
    if (1.5f * t_coef.z - t_bias.z > t_min) idx ^= 4u, pos.z = 1.5f;

    vec3 o_pos;
    vec4 o_col;

    while (scale < s_max) {
        if (child_descriptor == 0) child_descriptor = uOctree[parent];

        vec3 t_corner = pos * t_coef - t_bias;
        float tc_max = min(min(t_corner.x, t_corner.y), t_corner.z);

        uint child_shift = idx ^ octant_mask;
        uint child_masks = child_descriptor << child_shift;
        if ((child_masks & 0x8000) != 0 && t_min <= t_max) {

            // INTERSECT
            float tv_max = min(t_max, tc_max);
            float scale_exp2_half = scale_exp2 * 0.5f;
            vec3 t_center = scale_exp2_half * t_coef + t_corner;


            if (t_min <= tv_max) {

                if (WireFrameVoxel(o, d, pos, scale_exp2_half, o_pos, o_col)) {
                    hit_pos = o_pos;
                    return true;
                }
                // Leaf hit
                if ((child_masks & 0x0080) != 0) break;

                // PUSH
                if (tc_max < h) {
                    stack[scale] = uvec2(parent, floatBitsToUint(t_max));
                }
                h = tc_max;

                uint ofs = getChildPointer(child_descriptor);
                if ((child_descriptor & 0x10000) != 0) // far
                    ofs = parent + ofs * 2;
                ofs += bitCount(child_masks & 0x7f);
                parent += ofs * 2;

                idx = 0;
                scale--;
                scale_exp2 = scale_exp2_half;

                if (t_center.x > t_min) idx ^= 1, pos.x += scale_exp2;
                if (t_center.y > t_min) idx ^= 2, pos.y += scale_exp2;
                if (t_center.z > t_min) idx ^= 4, pos.z += scale_exp2;

                t_max = tv_max;
                child_descriptor = 0;
                continue;
            }
        }

        // ADVANCE
        uint step_mask = 0;
        if (t_corner.x <= tc_max) step_mask ^= 1, pos.x -= scale_exp2;
        if (t_corner.y <= tc_max) step_mask ^= 2, pos.y -= scale_exp2;
        if (t_corner.z <= tc_max) step_mask ^= 4, pos.z -= scale_exp2;

        t_min = tc_max;
        idx ^= step_mask;

        if ((idx & step_mask) != 0) {

            // POP
            uint differing_bits = 0;
            if ((step_mask & 1) != 0) differing_bits |= floatBitsToUint(pos.x) ^ floatBitsToUint(pos.x + scale_exp2);
            if ((step_mask & 2) != 0) differing_bits |= floatBitsToUint(pos.y) ^ floatBitsToUint(pos.y + scale_exp2);
            if ((step_mask & 4) != 0) differing_bits |= floatBitsToUint(pos.z) ^ floatBitsToUint(pos.z + scale_exp2);
            scale = (floatBitsToUint(float(differing_bits)) >> 23) - 127;
            scale_exp2 = uintBitsToFloat((scale - s_max + 127) << 23);

            uvec2 stackEntry = stack[scale].xy;
            parent = uOctree[stackEntry.x];
            t_max = uintBitsToFloat(stackEntry.y);

            // Rounding cube position and extract child slot index
            uvec3 sh = floatBitsToUint(pos) >> scale;
            pos = uintBitsToFloat(sh << scale);
            idx = (sh.x & 1) | ((sh.y & 1) << 1) | ((sh.z & 1) << 2);

            h = 0.f;
            child_descriptor = 0;
        }


    }


    if (scale >= s_max) {
        t_min = 2.f;
        return false;
    }


    if ((octant_mask & 1) == 0) pos.x = 3.f - scale_exp2 - pos.x;
    if ((octant_mask & 2) == 0) pos.y = 3.f - scale_exp2 - pos.y;
    if ((octant_mask & 4) == 0) pos.z = 3.f - scale_exp2 - pos.z;


    hit_t = t_min;
    hit_pos = min(max(o + t_min * d, pos + epsilon), pos + scale_exp2 - epsilon);

    //vec3 o_pos;
    //vec4 o_col;
    if (WireFrameVoxel(o, d, pos, scale_exp2, o_pos, o_col)) {
        hit_pos = o_pos;
        return true;
    } else {
        return false;
    }

    // return true;
}

bool WireFrameOctree(vec3 o, vec3 d, out vec3 o_pos, out vec4 o_color) {
    vec3 p = vec3(1.f);
    float t_min = -FLT_MAX, t_max = FLT_MAX;
    if (!IntersectVoxel(p, p + 1.f, o, d, t_min, t_max)) { // Intersect root
        return false;
    }

    uint stack[32];
    uint stack_ptr = 0;
    stack[stack_ptr++] = uOctree[root];
    float octree_scale = .5f;

    if (!WireFrameVoxel(o, d, p, 0.5f, o_pos, o_color)) {
        uint node = stack[--stack_ptr];
        octree_scale *= .5f;
        for (uint i = 0; i < 7; i++) {
            if (getValidBit(node, i) != 0) {
                t_min = -FLT_MAX, t_max = FLT_MAX;
                vec3 node_pos_min = p + BitmaskToVec3(i) * 2 * octree_scale;
                if (IntersectVoxel(node_pos_min, node_pos_min + octree_scale * 2, o, d, t_min, t_max)) {
                    if (WireFrameVoxel(o, d, p + BitmaskToVec3(i) * 2 * octree_scale, octree_scale, o_pos, o_color)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    return true;
}

#endif // OCTREE_GLSL