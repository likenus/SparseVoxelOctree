#include "noise.glsl"

const int N = 256;

// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/math.h#L727
// permute i amongst first n integers
uint permutationElement(uint i, const uint n, const uint seed) {
    uint w = n - 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    w |= w >> 16;
    do {
        i ^= seed;
        i *= 0xe170893d;
        i ^= seed >> 16;
        i ^= (i & w) >> 4;
        i ^= seed >> 8;
        i *= 0x0929eb3f;
        i ^= seed >> 23;
        i ^= (i & w) >> 1;
        i *= 1 | seed >> 27;
        i *= 0x6935fa69;
        i ^= (i & w) >> 11;
        i *= 0x74dcb303;
        i ^= (i & w) >> 2;
        i *= 0x9e501cc3;
        i ^= (i & w) >> 2;
        i *= 0xc860a3df;
        i &= w;
        i ^= i >> 5;
    } while (i >= n);
    return (i + seed) % n;
}

uint P(uint i) {
    return permutationElement(i, N, 0);
}

uint Index(vec3 v) {
    vec3 w = vec3(v) * 256.f;
    uvec3 u = uvec3(floor(w));
    return P(u.x + P(u.y + P(u.z)));
}

float noise(vec3 v) {
    return rtab[Index(v)];
}

float turbulence(vec3 x, const float frequency, uint k) {
    float sum = 0;
    float f = frequency;

    for (uint i = 0; i < k; i++) {
        sum += (1.f / f) * noise(uvec3(f * x));
        f *= frequency;
    }

    return sum;
}
