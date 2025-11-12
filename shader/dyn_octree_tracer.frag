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

    vec3 _pos;
    vec4 _col;
    oColor = vec4(0.f);
    if (Simple_Raymarch(o, d, _pos, _col)) {
        oColor = vec4(_pos, 1.f);
    }
}
