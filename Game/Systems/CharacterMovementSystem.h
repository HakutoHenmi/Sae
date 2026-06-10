#pragma once
#include "ISystem.h"
#include <cmath>

namespace Game {

class CharacterMovementSystem : public ISystem {
public:
	void Update(entt::registry& registry, GameContext& ctx) override {
		if (!ctx.isPlaying) return;

		auto view = registry.view<CharacterMovementComponent, RigidbodyComponent, TransformComponent>();
		for (auto entity : view) {
			auto& cm = view.get<CharacterMovementComponent>(entity);
			auto& rb = view.get<RigidbodyComponent>(entity);
			auto& tc = view.get<TransformComponent>(entity);
			if (!cm.enabled || !rb.enabled) continue;

			DirectX::XMFLOAT2 inputDir = {0, 0};
			bool wantJump = false;
			if (registry.all_of<PlayerInputComponent>(entity)) {
				auto& pi = registry.get<PlayerInputComponent>(entity);
				if (pi.enabled) {
					inputDir = pi.moveDir;
					wantJump = pi.jumpRequested;
				}
			}

			// --- カメラ基準の移動計算 ---
			// --- 3rd Person Character Controller 方式の移動更新 ---
			float moveX = 0, moveZ = 0;
			if (ctx.camera) {
				// 入力の正規化 (斜め移動の加速防止)
				float mag = std::sqrt(inputDir.x * inputDir.x + inputDir.y * inputDir.y);
				DirectX::XMFLOAT2 normInput = inputDir;
				if (mag > 1.0f) {
					normInput.x /= mag;
					normInput.y /= mag;
				}

				auto camRot = ctx.camera->Rotation();
				float cy = std::cos(camRot.y);
				float sy = std::sin(camRot.y);
				moveX = normInput.x * cy + normInput.y * sy;
				moveZ = -normInput.x * sy + normInput.y * cy;
			}
			
			// 1. 壁判定と水平移動
			float desiredX = moveX * cm.speed * ctx.dt;
			float desiredZ = moveZ * cm.speed * ctx.dt;

			if (ctx.scene && (std::abs(moveX) > 0.001f || std::abs(moveZ) > 0.001f)) {
				// --- 強力な段差制限による壁判定 ---
				float futureX = tc.translate.x + desiredX;
				float futureZ = tc.translate.z + desiredZ;
				float currentFeetY = tc.translate.y - cm.heightOffset;
				// 移動先の地面高さを先読み (startY は少し上から発射してすり抜け防止)
				float futureGround = ctx.scene->GetHeightAt(futureX, futureZ, tc.translate.y + 1.0f, static_cast<uint32_t>(entity));

				// 移動先が 0.4m 以上高いなら壁とみなして移動をブロック
				if (futureGround > currentFeetY + 0.4f) {
					desiredX = 0;
					desiredZ = 0;
				} else {
					// 膝くらいの高さから進行方向にレイを飛ばす (通常の壁判定も併用)
					Engine::Vector3 rayOrig = {tc.translate.x, tc.translate.y + 0.5f, tc.translate.z}; 
					Engine::Vector3 rayDir = {moveX, 0, moveZ};
					float hitDist = 0;
					if (ctx.scene->RayCast(rayOrig, rayDir, 0.6f, static_cast<uint32_t>(entity), hitDist)) {
						desiredX = 0;
						desiredZ = 0;
					}
				}
			}
			tc.translate.x += desiredX;
			tc.translate.z += desiredZ;

			// 2. 垂直移動と重力の更新 (isKinematic = true 前提での手動計算)
			if (cm.isGrounded) {
				if (wantJump) {
					rb.velocity.y = cm.jumpPower;
					cm.isGrounded = false;
				} else {
					rb.velocity.y = 0.0f; // 地面では垂直速度ゼロ
				}
			} else {
				// 自由落下
				rb.velocity.y -= cm.gravity * ctx.dt;
			}
			tc.translate.y += rb.velocity.y * ctx.dt;

			// 3. 接地判定とスナップ (レイキャストを使用)
			if (ctx.scene) {
				// 自身の位置から真下の地面高さを取得 (少し上から発射してすり抜け防止)
				float groundHeight = ctx.scene->GetHeightAt(tc.translate.x, tc.translate.z, tc.translate.y + 1.0f, static_cast<uint32_t>(entity));
				
				// 接地判定ロジックの刷新: 
				// 1. 上昇中 (rb.velocity.y > 0.01) は絶対に接地させない (多段ジャンプ防止)
				// 2. 落下または静止中 (rb.velocity.y <= 0.01) かつ 地面を突き抜けた(埋まった)場合のみ着地・固定
				// 3. 地面から離れている場合は grounded を解除 (空中浮遊防止)
				
				float feetY = tc.translate.y - cm.heightOffset;
				if (rb.velocity.y <= 0.01f) {
					// 地面を通過したか、ほぼ地表にある場合のみ snap
					if (feetY <= groundHeight + 0.01f && groundHeight > -5000.0f) {
						tc.translate.y = groundHeight + cm.heightOffset;
						rb.velocity.y = 0.0f;
						cm.isGrounded = true;
					} else {
						cm.isGrounded = false;
					}
				} else {
					// 打ち上げ中
					cm.isGrounded = false;
				}
			}

			// --- 4. 回転 (スムーズな補間) ---
			if (std::abs(moveX) > 0.01f || std::abs(moveZ) > 0.01f) {
				float targetRotation = std::atan2(moveX, moveZ);
				float currentRotation = tc.rotate.y;
				
				// 最短角補間
				float diff = targetRotation - currentRotation;
				while (diff >  DirectX::XM_PI) diff -= DirectX::XM_2PI;
				while (diff < -DirectX::XM_PI) diff += DirectX::XM_2PI;
				
				float rotationSpeed = 20.0f; 
				tc.rotate.y += diff * std::min(1.0f, rotationSpeed * ctx.dt);
			}


		}
	}

	void Reset(entt::registry& registry) override {
		auto view = registry.view<CharacterMovementComponent>();
		for (auto entity : view) {
			auto& cm = registry.get<CharacterMovementComponent>(entity);
			cm.isGrounded = false;
		}
	}
};

} // namespace Game
