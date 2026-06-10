#pragma once
// ===============================
//  Camera : デバッグ操作 + 俯瞰用API + 画面シェイク
//  - Initialize/Update/SetProjection は既存のまま
//  - SetPosition / LookAt / Tick / StartShake を追加
// ===============================
#include "Input.h"
#include <DirectXMath.h>
#include <random>
#include "Matrix4x4.h"

namespace Engine {

class Camera {
public:
	// 初期化（位置/回転を既定値へ）
	void Initialize();

	// デバッグ用の自由視点更新（WASD+QE、矢印）※既存APIは維持
	void Update(const Input& in);

	// 1フレーム進める（シェイク等の時間依存処理）
	void Tick(float dt);

	// ===== 行列取得 =====
	DirectX::XMMATRIX View() const { return view_; }
	DirectX::XMMATRIX Proj() const { return proj_; }

	// ===== プロジェクション設定（既存） =====
	void SetProjection(float fovY, float aspect, float nearZ, float farZ);

	// ===== 位置・注視 API（追加/既存） =====
	void SetPosition(const DirectX::XMFLOAT3& p);
	void SetPosition(float x, float y, float z);
	DirectX::XMFLOAT3 Position() const { return pos_; }
	Engine::Vector3 GetPosition() const { return Engine::Vector3{pos_.x, pos_.y, pos_.z}; }

	// LookAt でビュー行列を直接設定
	void LookAt(const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);
	void LookAt(float tx, float ty, float tz, float ux, float uy, float uz);

	// 回転の取得/設定
	DirectX::XMFLOAT3 Rotation() const { return rot_; }
	Engine::Vector3 GetRotation() const { return Vector3{rot_.x, rot_.y, rot_.z}; }
	void SetRotation(const DirectX::XMFLOAT3& r);
	void SetRotation(float pitch, float yaw, float roll);

	// ===== 画面シェイク =====
	// ampPos: 位置ノイズの最大量（ワールド座標単位）、ampRot: 回転ノイズの最大量（ラジアン）
	void StartShake(float duration, float ampPos, float ampRot = 0.0f);
	void StopShake();
	bool IsShaking() const { return shakeTime_ < shakeDuration_; }

private:
	// 位置・回転(+シェイク)からビューを再計算
	void UpdateView();

private:
	// 位置・回転（右手系相当: pitch=X, yaw=Y, roll=Z）
	DirectX::XMFLOAT3 pos_{8.0f, 10.0f, -20.0f};
	DirectX::XMFLOAT3 rot_{0, 0, 0};

	// デバッグ挙動
	float moveSpd_ = 0.10f;
	float rotSpd_ = DirectX::XMConvertToRadians(1.0f);

	// 行列
	DirectX::XMMATRIX view_{};
	DirectX::XMMATRIX proj_{};

	// ===== シェイク内部状態 =====
	float shakeTime_ = 9999.0f;  // 進行時間
	float shakeDuration_ = 0.0f; // 全体の長さ
	float shakeAmpPos_ = 0.0f;   // 位置ノイズ振幅
	float shakeAmpRot_ = 0.0f;   // 回転ノイズ振幅（ラジアン）

	DirectX::XMFLOAT3 shakeOfs_{0, 0, 0}; // 直近フレームの位置ノイズ
	DirectX::XMFLOAT3 shakeRot_{0, 0, 0}; // 直近フレームの回転ノイズ

	// 乱数
	std::mt19937 rng_{std::random_device{}()};
	std::uniform_real_distribution<float> unit_{-1.0f, 1.0f};
};

} // namespace Engine
