// Matrix4x4.h
#pragma once
#include <DirectXMath.h>
#include <cmath>
using namespace DirectX;

namespace Engine {

struct Vector2 {
	float x, y;
};

struct Vector3 {
	float x, y, z;

	   // 加算
	Vector3 operator+(const Vector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
	Vector3& operator+=(const Vector3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}

	// 減算
	Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
	Vector3& operator-=(const Vector3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	// スカラー倍
	Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
	Vector3& operator*=(float s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}

	// スカラー除算
	Vector3 operator/(float s) const { return {x / s, y / s, z / s}; }


};

struct Vector4 {
	float x, y, z, w;
};



class Matrix4x4 {
public:
	float m[4][4]{};

	static Matrix4x4 Identity() {
		Matrix4x4 mat{};
		for (int i = 0; i < 4; i++)
			mat.m[i][i] = 1.0f;
		return mat;
	}

	static Matrix4x4 MakeScaleMatrix(const Vector3& s) {
		XMMATRIX xm = XMMatrixScaling(s.x, s.y, s.z);
		return FromXM(xm);
	}

	static Matrix4x4 MakeTranslateMatrix(const Vector3& t) {
		XMMATRIX xm = XMMatrixTranslation(t.x, t.y, t.z);
		return FromXM(xm);
	}

	static Matrix4x4 MakeRotateXYZMatrix(const Vector3& r) {
		XMMATRIX xm = XMMatrixRotationRollPitchYaw(r.x, r.y, r.z);
		return FromXM(xm);
	}

	static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
		XMMATRIX ma = ToXM(a);
		XMMATRIX mb = ToXM(b);
		return FromXM(XMMatrixMultiply(ma, mb));
	}

	static Matrix4x4 PerspectiveFov(float fovY, float aspect, float nearZ, float farZ) { return FromXM(XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ)); }

	static Matrix4x4 MakeAffineMatrix(const Vector3& s, const Vector3& r, const Vector3& t) {
		XMMATRIX xm = XMMatrixScaling(s.x, s.y, s.z) * XMMatrixRotationRollPitchYaw(r.x, r.y, r.z) * XMMatrixTranslation(t.x, t.y, t.z);
		return FromXM(xm);
	}

	static Matrix4x4 Inverse(const Matrix4x4& m) {
		XMMATRIX xm = ToXM(m);
		XMVECTOR det;
		return FromXM(XMMatrixInverse(&det, xm));
	}

	

private:
	static Matrix4x4 FromXM(FXMMATRIX xm) {
		Matrix4x4 out;
		XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), xm);
		return out;
	}
	static XMMATRIX ToXM(const Matrix4x4& m) { return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&m)); }
};

// ========================================
// Vector3用のNormalize関数（非メンバ関数）
// ========================================
inline Vector3 Normalize(const Vector3& v) {
	float mag = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (mag > 0.0001f) {
		return {v.x / mag, v.y / mag, v.z / mag};
	}
	return {0, 0, 0};
}

//線形補間
inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return Vector3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }

// プリズム反射（Reflect＋45度方向ずらす例）
inline Vector3 Prizm(const Vector3& dir, const Vector3& normal) {
	// まず通常の反射
	float dot = dir.x * normal.x + dir.y * normal.y + dir.z * normal.z;
	Vector3 r = {dir.x - 2 * dot * normal.x, dir.y - 2 * dot * normal.y, dir.z - 2 * dot * normal.z};

	// Y軸方向に45度回転
	float s = std::sin(XMConvertToRadians(45.0f));
	float c = std::cos(XMConvertToRadians(45.0f));
	Vector3 rotated = {r.x * c - r.z * s, r.y, r.x * s + r.z * c};
	return Normalize(rotated);
}

	// 内積
inline float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vector3 Cross(const Vector3& a, const Vector3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

inline Matrix4x4 MakeRotateAxisAngle(const Vector3& axis, float angle) {
	using namespace DirectX;
	// 正規化した軸で回転行列を作る
	XMVECTOR ax = XMVector3Normalize(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&axis)));
	XMMATRIX xm = XMMatrixRotationAxis(ax, angle);
	Matrix4x4 out{};
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), xm);
	return out;
}

inline Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m) {
	// 平行移動成分は加えない（法線／方向ベクトル用）
	return {v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0], v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1], v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]};
}

inline Vector3 TransformCoord(const Vector3& v, const Matrix4x4& m) {
	float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
	return {
		(v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0]) / w,
		(v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1]) / w,
		(v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]) / w
	};
}

// 方向ベクトルからオイラー角（X,Y,Z軸回転）を計算する（Z軸が正面の場合）
inline Vector3 LookRotation(const Vector3& direction, const Vector3& up = {0.0f, 1.0f, 0.0f}) {
	using namespace DirectX;
	XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&direction)));
	if (XMVectorGetX(XMVector3LengthSq(dir)) < 1e-6f) return {0, 0, 0};
	
	XMVECTOR upVec = XMVector3Normalize(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&up)));
	
	// もし方向ベクトルが真上や真下を向いている場合の特例処理
	if (std::abs(XMVectorGetX(XMVector3Dot(dir, upVec))) > 0.999f) {
		upVec = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Fallback up
	}

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(upVec, dir));
	XMVECTOR realUp = XMVector3Normalize(XMVector3Cross(dir, right));

	XMFLOAT4X4 rotMat;
	XMStoreFloat4x4(&rotMat, XMMatrixIdentity());
	rotMat._11 = XMVectorGetX(right); rotMat._12 = XMVectorGetY(right); rotMat._13 = XMVectorGetZ(right);
	rotMat._21 = XMVectorGetX(realUp); rotMat._22 = XMVectorGetY(realUp); rotMat._23 = XMVectorGetZ(realUp);
	rotMat._31 = XMVectorGetX(dir); rotMat._32 = XMVectorGetY(dir); rotMat._33 = XMVectorGetZ(dir);

	// 回転テンソルからオイラー角（Pitch, Yaw, Roll）への変換
	float pitch = std::asin(-rotMat._32);
	float yaw = 0.0f;
	float roll = 0.0f;

	if (std::cos(pitch) > 1e-4f) {
		yaw = std::atan2(rotMat._31, rotMat._33);
		roll = std::atan2(rotMat._12, rotMat._22);
	} else {
		yaw = std::atan2(-rotMat._13, rotMat._11);
		roll = 0.0f;
	}

	return {pitch, yaw, roll};
}

} // namespace Engine