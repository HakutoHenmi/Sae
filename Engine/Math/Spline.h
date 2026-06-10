#pragma once
#include <vector>
#include <DirectXMath.h>
#include "../Matrix4x4.h"

namespace Engine {

class Spline {
public:
    // Catmull-Rom interpolation for Vector3
    // t is [0, 1] between p1 and p2
    static DirectX::XMVECTOR CatmullRom(
        const DirectX::XMVECTOR& p0,
        const DirectX::XMVECTOR& p1,
        const DirectX::XMVECTOR& p2,
        const DirectX::XMVECTOR& p3,
        float t) 
    {
        return DirectX::XMVectorCatmullRom(p0, p1, p2, p3, t);
    }

    // Interpolate points in a list
    // t is [0, count-1]
    static DirectX::XMVECTOR Interpolate(const std::vector<DirectX::XMFLOAT3>& points, float t) {
        if (points.empty()) return DirectX::XMVectorZero();
        if (points.size() == 1) return DirectX::XMLoadFloat3(&points[0]);

        int count = static_cast<int>(points.size());
        int i = static_cast<int>(t);
        if (i >= count - 1) return DirectX::XMLoadFloat3(&points[count - 1]);
        if (i < 0) return DirectX::XMLoadFloat3(&points[0]);

        float localT = t - static_cast<float>(i);

        DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&points[i == 0 ? 0 : i - 1]);
        DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&points[i]);
        DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&points[i + 1]);
        DirectX::XMVECTOR p3 = DirectX::XMLoadFloat3(&points[i + 2 >= count ? count - 1 : i + 2]);

        return CatmullRom(p0, p1, p2, p3, localT);
    }
};

} // namespace Engine
