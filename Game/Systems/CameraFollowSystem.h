#include "ISystem.h"
#include <cmath>
#include <algorithm>
#include "../Engine/Input.h"

namespace Game {

class CameraFollowSystem : public ISystem {
private:
	// AABBとレイの交差判定 (簡単なスプリングアーム用)
	static float RaycastAABB(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDir,
	                         const TransformComponent& tc, const BoxColliderComponent& bc) {
		// ワールド空間のAABB（回転なしを仮定するか、AABBを最大化して計算）
		float cx = tc.translate.x + bc.center.x;
		float cy = tc.translate.y + bc.center.y;
		float cz = tc.translate.z + bc.center.z;
		// AABBの半分
		float hx = bc.size.x * 0.5f * tc.scale.x;
		float hy = bc.size.y * 0.5f * tc.scale.y;
		float hz = bc.size.z * 0.5f * tc.scale.z;

		float minX = cx - hx, maxX = cx + hx;
		float minY = cy - hy, maxY = cy + hy;
		float minZ = cz - hz, maxZ = cz + hz;

		float t1 = (minX - rayOrigin.x) / (rayDir.x != 0 ? rayDir.x : 1e-5f);
		float t2 = (maxX - rayOrigin.x) / (rayDir.x != 0 ? rayDir.x : 1e-5f);
		float t3 = (minY - rayOrigin.y) / (rayDir.y != 0 ? rayDir.y : 1e-5f);
		float t4 = (maxY - rayOrigin.y) / (rayDir.y != 0 ? rayDir.y : 1e-5f);
		float t5 = (minZ - rayOrigin.z) / (rayDir.z != 0 ? rayDir.z : 1e-5f);
		float t6 = (maxZ - rayOrigin.z) / (rayDir.z != 0 ? rayDir.z : 1e-5f);

		float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
		float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

		// 衝突しなかったか、レイの後ろで衝突した場合
		if (tmax < 0 || tmin > tmax) return -1.0f;
		return tmin; // 交差距離
	}

public:
	void Update(entt::registry& registry, GameContext& ctx) override {
		if (!ctx.isPlaying || !ctx.camera) return;

		auto view = registry.view<CameraTargetComponent, TransformComponent>();
		for (auto entity : view) {
			auto& ct = view.get<CameraTargetComponent>(entity);
			if (!ct.enabled) continue;

			// ★追加: マウスホイールによるズーム
			auto* inputIns = ::Engine::Input::GetInstance();
			if (inputIns) {
				float wheel = inputIns->GetMouseWheelDelta();
				if (std::abs(wheel) > 0.001f) {
					ct.distance -= wheel * 0.005f; // 感度調整
					ct.distance = std::clamp(ct.distance, 3.0f, 20.0f); // 範囲制限
				}
			}

			auto& tc = view.get<TransformComponent>(entity);
			DirectX::XMFLOAT3 targetPos = tc.translate;

			if (registry.all_of<PlayerInputComponent>(entity)) {
				auto& pi = registry.get<PlayerInputComponent>(entity);
				if (pi.enabled) {
					auto rot = ctx.camera->Rotation();
					
					// ★追加: ロックオン中の場合、カメラを敵の方に向ける
					if (pi.lockedEnemy != entt::null && registry.valid(pi.lockedEnemy)) {
						if (registry.all_of<TransformComponent>(pi.lockedEnemy)) {
							auto& eTc = registry.get<TransformComponent>(pi.lockedEnemy);
							// 敵の少し上（体の中央あたり）を注視点にする
							float targetY = eTc.translate.y + 1.5f;
							
							float dx = eTc.translate.x - targetPos.x;
							float dy = targetY - (targetPos.y + ct.height);
							float dz = eTc.translate.z - targetPos.z;
							
							float targetYaw = std::atan2(dx, dz);
							float horizontalDist = std::sqrt(dx * dx + dz * dz);
							float targetPitch = -std::atan2(dy, horizontalDist);
							
							// スムーズに追従
							float lerpSpeed = 10.0f * ctx.dt;
							
							// Yawの最短角度補間
							float diffYaw = targetYaw - rot.y;
							while (diffYaw < -DirectX::XM_PI) diffYaw += DirectX::XM_2PI;
							while (diffYaw > DirectX::XM_PI) diffYaw -= DirectX::XM_2PI;
							rot.y += diffYaw * lerpSpeed;
							
							rot.x += (targetPitch - rot.x) * lerpSpeed;
						}
					} else {
						// 通常のマウス操作
						rot.y += pi.cameraYaw;
						rot.x += pi.cameraPitch;
					}

					const float PITCH_LIMIT = 1.5f;
					if (rot.x > PITCH_LIMIT) rot.x = PITCH_LIMIT;
					if (rot.x < -PITCH_LIMIT) rot.x = -PITCH_LIMIT;

					ctx.camera->SetRotation(rot);
				}
			}

			auto curRot = ctx.camera->Rotation();
			float camSy = std::sin(curRot.y);
			float camCy = std::cos(curRot.y);
			float camSx = std::sin(curRot.x);
			float camCx = std::cos(curRot.x);

			DirectX::XMFLOAT3 offset = {
				-camSy * camCx * ct.distance,
				ct.height + camSx * ct.distance,
				-camCy * camCx * ct.distance
			};

			DirectX::XMFLOAT3 desiredPos = {
				targetPos.x + offset.x,
				targetPos.y + offset.y,
				targetPos.z + offset.z
			};

			// ★追加: スプリングアーム（レイキャストによる壁めり込み防止）
			DirectX::XMFLOAT3 rayDir = { desiredPos.x - targetPos.x, desiredPos.y - targetPos.y, desiredPos.z - targetPos.z };
			float distToDesired = std::sqrt(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);
			
			if (distToDesired > 0.001f) {
				rayDir.x /= distToDesired; rayDir.y /= distToDesired; rayDir.z /= distToDesired;
				
				float minHitDist = distToDesired;
				auto walls = registry.view<BoxColliderComponent, TransformComponent, TagComponent>();
				for (auto w : walls) {
					// TagType::Wall のみ判定する（ボス自身などを避けたい場合はWallタグのみにする）
					if (walls.get<TagComponent>(w).tag != TagType::Wall) continue;
					
					auto& bc = walls.get<BoxColliderComponent>(w);
					auto& wtc = walls.get<TransformComponent>(w);
					
					float hitT = RaycastAABB(targetPos, rayDir, wtc, bc);
					if (hitT >= 0.0f && hitT < minHitDist) {
						minHitDist = hitT;
					}
				}

				// 壁に当たった場合、カメラを壁の少し手前に寄せる
				if (minHitDist < distToDesired) {
					float margin = 0.3f; // 壁からのマージン
					float safeDist = std::max(0.0f, minHitDist - margin);
					desiredPos = {
						targetPos.x + rayDir.x * safeDist,
						targetPos.y + rayDir.y * safeDist,
						targetPos.z + rayDir.z * safeDist
					};
				}
			}

			DirectX::XMFLOAT3 currentPos = ctx.camera->Position();
			float t = ct.smoothSpeed * ctx.dt;
			if (t > 1.0f) t = 1.0f;

			DirectX::XMFLOAT3 newPos = {
				currentPos.x + (desiredPos.x - currentPos.x) * t,
				currentPos.y + (desiredPos.y - currentPos.y) * t,
				currentPos.z + (desiredPos.z - currentPos.z) * t
			};

			ctx.camera->SetPosition(newPos);
			break;
		}
	}
};

} // namespace Game
