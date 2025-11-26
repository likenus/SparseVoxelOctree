#version 450

#define DYN_OCTREE_SET 0
#include "dyn_octree.glsl"
#define CAMERA_SET 1
#include "camera.glsl"
#define ENVIRONMENT_MAP_SET 2
#define ENVIRONMENT_MAP_ENABLE_SAMPLE 0
#include "environment_map.glsl"

layout(push_constant) uniform uuPushConstant {
    uint uWidth, uHeight;
};

layout(location = 0) out vec4 oColor;

void main() {
    vec3 o = uPosition.xyz, d = Camera_GenRay(ivec2(gl_FragCoord.xy) / vec2(uWidth, uHeight));

    vec3 o_pos;
    vec4 o_col;
    oColor = vec4(0.f);
    if (SimpleRaymarch(o, d, o_pos, o_col)) {
        oColor = vec4(o_pos - 1.f, 1.f);
    } else {
        oColor = vec4(1.f);
    }


    float t;
    if (RaycastOctree(o, d, t, o_pos)) {
        oColor = vec4(o_pos - 1.f, 1.f);
    }

    /*
    if (WireFrameOctree(o, d, o_pos, o_col)) {
        oColor = vec4(o_pos - 1.f, 1.f);
    }
*/
}
