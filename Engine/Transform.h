#pragma once
#include "Matrix4x4.h"
#include <cmath>

namespace Engine {

struct EulerTransform {
	Vector3 scale{1, 1, 1};
	Vector3 rotate{0, 0, 0};
	Vector3 translate{0, 0, 0};

	Matrix4x4 ToMatrix() const {
		using namespace DirectX;
		XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
		XMMATRIX T = XMMatrixTranslation(translate.x, translate.y, translate.z);
		XMMATRIX M;
		// 回転がほぼゼロなら回転行列の生成をスキップして高速化
		if (std::abs(rotate.x) < 1e-6f && std::abs(rotate.y) < 1e-6f && std::abs(rotate.z) < 1e-6f) {
			M = S * T;
		} else {
			XMMATRIX R = XMMatrixRotationRollPitchYaw(rotate.x, rotate.y, rotate.z);
			M = S * R * T;
		}
		Matrix4x4 out;
		XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), M);
		return out;
	}
};

struct QuaternionTransform {
	Vector3 scale{1, 1, 1};
	DirectX::XMFLOAT4 rotate{0, 0, 0, 1};
	Vector3 translate{0, 0, 0};

	Matrix4x4 ToMatrix() const {
		using namespace DirectX;
		XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
		XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&rotate));
		XMMATRIX T = XMMatrixTranslation(translate.x, translate.y, translate.z);
		XMMATRIX M = S * R * T;
		Matrix4x4 out;
		XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), M);
		return out;
	}
};

// 後方互換性のため
using Transform = EulerTransform;

} // namespace Engine
