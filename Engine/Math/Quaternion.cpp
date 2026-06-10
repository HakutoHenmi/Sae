#include "Quaternion.h"



Engine::Quaternion Engine::Multiply(const Quaternion& lhs, const Quaternion& rhs) {

	Quaternion result;

	result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
	result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
	result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
	result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;

	return result;
}


float Engine::Norm(const Engine::Quaternion& quaternion) {
	return std::sqrt(quaternion.w * quaternion.w + quaternion.x * quaternion.x + quaternion.y * quaternion.y + quaternion.z * quaternion.z); 
}

Engine::Quaternion Engine::IdentityQuaternion() {
	Engine::Quaternion result;

	result.w = 1.0f;
	result.x = 0.0f;
	result.y = 0.0f;
	result.z = 0.0f;

	return result;
}

Engine::Quaternion Engine::Normalize(const Engine::Quaternion& quaternion) {
	Quaternion result;

	float n = Norm(quaternion);
	if (n < 1e-6f) {
		// ゼロ割防止：回転なしにする
		return IdentityQuaternion();
	}

	result.w = quaternion.w / n;
	result.x = quaternion.x / n;
	result.y = quaternion.y / n;
	result.z = quaternion.z / n;

	return result;
}

Engine::Quaternion Engine::MakeRotateAxisIngleQuaternion(const Engine::Vector3& axis, float angle) {

	
	Engine::Quaternion q;

	// 軸がゼロに近いときは回転なし
	float len = Length(axis);
	if (len < 0.000001f) {
		q.x = q.y = q.z = 0.0f;
		q.w = 1.0f;
		return q;
	}

	Engine::Vector3 n = axis / len; // 正規化
	float half = angle * 0.5f;
	float s = sinf(half);

	q.x = n.x * s;
	q.y = n.y * s;
	q.z = n.z * s;
	q.w = cosf(half);

	return q;
}

Engine::Quaternion Engine::MakeRotateXYZQuaternion(const Engine::Vector3& euler) {
	float hx = euler.x * 0.5f;
	float hy = euler.y * 0.5f;
	float hz = euler.z * 0.5f;

	float sx = sinf(hx);
	float cx = cosf(hx);
	float sy = sinf(hy);
	float cy = cosf(hy);
	float sz = sinf(hz);
	float cz = cosf(hz);

	Engine::Quaternion q;

	// X → Y → Z の順
	q.w = cx * cy * cz + sx * sy * sz;
	q.x = sx * cy * cz - cx * sy * sz;
	q.y = cx * sy * cz + sx * cy * sz;
	q.z = cx * cy * sz - sx * sy * cz;

	return q;
}

Engine::Vector3 Engine::QuaternionToEuler(const Engine::Quaternion& q) {
	Vector3 euler;

	// X回転（Pitch）
	float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	euler.x = atan2f(sinr_cosp, cosr_cosp);

	// Y回転（Yaw）
	float sinp = 2.0f * (q.w * q.y - q.z * q.x);
	if (fabsf(sinp) >= 1.0f) {
		euler.y = copysignf(3.14f * 0.5f, sinp); // 90度固定
	} else {
		euler.y = asinf(sinp);
	}

	// Z回転（Roll）
	float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	euler.z = atan2f(siny_cosp, cosy_cosp);

	return euler;
}

