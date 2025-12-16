#version 450
#define OCTREE_SET 0
#include "octree.glsl"
#define CAMERA_SET 1
#include "camera.glsl"
#define ENVIRONMENT_MAP_SET 2
#define ENVIRONMENT_MAP_ENABLE_SAMPLE 0
#include "environment_map.glsl"

#define EPS 1e-7

layout(set = 3, binding = 0) uniform sampler2D uBeamImage;
layout(location = 0) out vec4 oColor;

layout(push_constant) uniform uuPushConstant {
	uint uWidth, uHeight, uViewType, uLightType, uBeamEnable, uBeamSize;
	float uConstColor[3], uEnvMapRotation;
};

vec3 Heat(in float x) { return sin(clamp(x, 0.0, 1.0) * 3.0 - vec3(1, 2, 3)) * 0.5 + 0.5; }

vec3 Light(in vec3 d) {
	return uLightType == 0 ? vec3(uConstColor[0], uConstColor[1], uConstColor[2]) : EnvMap_Radiance(d, uEnvMapRotation);
}

void main() {
	vec3 o = uPosition.xyz, d = Camera_GenRay(ivec2(gl_FragCoord.xy) / vec2(uWidth, uHeight));

	float beam;
	if (uBeamEnable == 1) {
		ivec2 beam_coord = ivec2(gl_FragCoord.xy / uBeamSize);
		beam = min(min(texelFetch(uBeamImage, beam_coord, 0).r, texelFetch(uBeamImage, beam_coord + ivec2(1, 0), 0).r),
		           min(texelFetch(uBeamImage, beam_coord + ivec2(0, 1), 0).r,
		               texelFetch(uBeamImage, beam_coord + ivec2(1, 1), 0).r));
		o += d * beam;
	}

	vec3 pos, color, normal;
	uint iter, depth;
	bool hit = Octree_RayMarchLeaf(o, d, pos, color, normal, iter, depth);
	if (!hit) {
		pos = vec3(1.0);
		normal = vec3(0.0);
		color = Light(d);
	} else { // cast shadow
		vec3 l = normalize(vec3(2.f, 1.f, 1.f));
		o = pos + l * EPS;
		vec3 _pos, _col, _normal;
		uint _iter, _depth;
		if (Octree_RayMarchLeaf(o, l, _pos, _col, _normal, _iter, _depth)) {
			color *= (0.75f + .25f * dot(l, -d));
		}
	}

	switch (uViewType) {
		case 0:
			oColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
			break;
		case 1:
			oColor = vec4(normal * 0.5 + 0.5, 1.0);
			break;
		case 2:
			oColor = vec4(pos - 1.0, 1.0);
			break;
		case 3:
			oColor = vec4(Heat(iter / 128.0), 1.0);
			break;
		case 4:
			oColor = vec4(Heat(depth / 128.f), 1.0);
			break;
		default:
			oColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
	}
}
