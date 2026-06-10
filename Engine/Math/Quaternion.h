#pragma once
#include <Matrix4x4.h>


namespace Engine {

class Quaternion {
public:
    float x;
    float y;
    float z;
    float w;
};

Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);

float Norm(const Engine::Quaternion& quaternion);

Engine::Quaternion IdentityQuaternion();

Quaternion Normalize(const Quaternion& quaternion);

Quaternion MakeRotateAxisIngleQuaternion(const Engine::Vector3& axis, float angle);

Quaternion MakeRotateXYZQuaternion(const Engine::Vector3& euler);

Vector3 QuaternionToEuler(const Quaternion& q);

} // namespace Engine


