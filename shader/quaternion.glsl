#ifndef QUATERNION_UTIL
#define QUATERNION_UTIL

vec4 quat_mult(vec4 a, vec4 b) {
    return vec4(
        a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w,
        a.x * b.y + a.y * b.x + a.z * b.w - a.w * b.z,
        a.x * b.z - a.y * b.w + a.z * b.x + a.w * b.y,
        a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x
    );
}

vec4 quat_conjugate(vec4 q) {
    return q * vec4(1.f, -1.f, -1.f, -1.f);
}

// Rotate v around axis cw by phi degrees
vec3 quat_rotate(vec3 v, float phi, vec3 axis) {
    vec4 r = vec4(cos(phi / 2.f), axis * sin(phi / 2.f));
    return quat_mult(quat_mult(r, vec4(0.f, v)), quat_conjugate(r)).yzw;
}

// Create cw rotation around axis with phi degrees
vec4 quat_rotate(float phi, vec3 axis) {
    return vec4(cos(phi / 2.f), axis * sin(phi / 2.f));
}

// Rotate v using r
vec3 quat_rotate(vec3 v, vec4 r) {
    return quat_mult(quat_mult(r, vec4(0.f, v)), quat_conjugate(r)).yzw;
}

#endif