//
// Created by Linus on 18.12.2025.
//

#ifndef SPARSEVOXELOCTREE_QUATERNION_HPP
#define SPARSEVOXELOCTREE_QUATERNION_HPP

#include "glm/vec3.hpp"
#include "glm/glm.hpp"

class Quaternion : glm::vec4 {

public:
    Quaternion() = default;

    Quaternion(float x, float y, float z, float w) : glm::vec4(x, y, z, w) {}
    Quaternion(float x, glm::vec3 yzw) : glm::vec4(x, yzw) {}
    Quaternion(glm::vec3 xyz, float w) : glm::vec4(xyz, w) {}

    friend Quaternion operator*(Quaternion a, const Quaternion &b) {
        a = Quaternion(
            a.x * b.x - a.y * b.y - a.z * b.z - a.w * b.w,
            a.x * b.y + a.y * b.x + a.z * b.w - a.w * b.z,
            a.x * b.z - a.y * b.w + a.z * b.x + a.w * b.y,
            a.x * b.w + a.y * b.z - a.z * b.y + a.w * b.x);
        return a;
    }

    Quaternion conjugate() {
        return Quaternion(x, -y, -z, -w);
    }

    static glm::vec3 rotate(glm::vec3 v, float phi, glm::vec3 axis) {
        Quaternion r = Quaternion(glm::cos(phi /2.f), axis * glm::sin(phi / 2.f));
        Quaternion w = Quaternion(0.f, v);

        auto res = r * w * r.conjugate();
        return glm::vec3(res.y, res.z, res.w);
    }

    static glm::vec3 rotate(glm::vec3 v, Quaternion r) {
        Quaternion w = Quaternion(0.f, v);

        auto res = r * w * r.conjugate();
        return glm::vec3(res.x, res.y, res.z);
    }
};


#endif //SPARSEVOXELOCTREE_QUATERNION_HPP