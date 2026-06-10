#pragma once
#include "ISystem.h"
#include <cmath>
#include <cfloat>
#include <vector>
#include <algorithm>
#include "../Engine/QuadTree.h"

namespace Game {

class PhysicsSystem : public ISystem {
public:
	void Update(entt::registry& registry, GameContext& ctx) override {
		if (!ctx.isPlaying) return;

		// --- 事前準備: エンティティのフィルタリングとAABB事前計算 ---
		m_dynamics.clear();
		m_statics.clear();

		// 動的オブジェクト (Rigidbody + BoxCollider + Transform)
		auto dynamicView = registry.view<RigidbodyComponent, BoxColliderComponent, TransformComponent>();
		for (auto entity : dynamicView) {
			auto& rb = dynamicView.get<RigidbodyComponent>(entity);
			auto& bc = dynamicView.get<BoxColliderComponent>(entity);
			auto& tc = dynamicView.get<TransformComponent>(entity);
			if (!rb.enabled || !bc.enabled) continue;

			float damping = 3.0f;
			bool isGrounded = false;
			bool hasCMS = false;
			if (auto* cm = registry.try_get<CharacterMovementComponent>(entity)) {
				isGrounded = cm->isGrounded;
				hasCMS = true;
			}

			if (rb.useGravity && !isGrounded && !hasCMS) {
				float gravity = 9.8f;
				rb.velocity.y -= gravity * ctx.dt;
			}
			if (!rb.isKinematic) {
				rb.velocity.x -= rb.velocity.x * damping * ctx.dt;
				rb.velocity.z -= rb.velocity.z * damping * ctx.dt;

				tc.translate.x += rb.velocity.x * ctx.dt;
				if (!hasCMS) {
					tc.translate.y += rb.velocity.y * ctx.dt;
				}
				tc.translate.z += rb.velocity.z * ctx.dt;
			}

			CollidableBox cb;
			cb.entity = entity;
			::Engine::Matrix4x4 world = ctx.scene ? ctx.scene->GetWorldMatrix(static_cast<int>(entity)) : tc.ToMatrix();
			GetObbAxes(world, bc, cb.axes, cb.center, cb.extents);
			
			cb.aabbMin = {FLT_MAX, FLT_MAX, FLT_MAX};
			cb.aabbMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
			for (int k = 0; k < 8; ++k) {
				Engine::Vector3 p = cb.center;
				p.x += ((k & 1) ? 1 : -1) * cb.axes[0].x * cb.extents.x + ((k & 2) ? 1 : -1) * cb.axes[1].x * cb.extents.y + ((k & 4) ? 1 : -1) * cb.axes[2].x * cb.extents.z;
				p.y += ((k & 1) ? 1 : -1) * cb.axes[0].y * cb.extents.x + ((k & 2) ? 1 : -1) * cb.axes[1].y * cb.extents.y + ((k & 4) ? 1 : -1) * cb.axes[2].y * cb.extents.z;
				p.z += ((k & 1) ? 1 : -1) * cb.axes[0].z * cb.extents.x + ((k & 2) ? 1 : -1) * cb.axes[1].z * cb.extents.y + ((k & 4) ? 1 : -1) * cb.axes[2].z * cb.extents.z;
				cb.aabbMin.x = std::min(cb.aabbMin.x, p.x); cb.aabbMin.y = std::min(cb.aabbMin.y, p.y); cb.aabbMin.z = std::min(cb.aabbMin.z, p.z);
				cb.aabbMax.x = std::max(cb.aabbMax.x, p.x); cb.aabbMax.y = std::max(cb.aabbMax.y, p.y); cb.aabbMax.z = std::max(cb.aabbMax.z, p.z);
			}
			m_dynamics.push_back(cb);
		}

		// 静的オブジェクト (GpuMeshCollider + Transform)
		auto staticView = registry.view<GpuMeshColliderComponent, TransformComponent>();
		for (auto entity : staticView) {
			auto& gmc = staticView.get<GpuMeshColliderComponent>(entity);
			auto& tc = staticView.get<TransformComponent>(entity);
			if (!gmc.enabled || !ctx.renderer) continue;

			auto* model = ctx.renderer->GetModel(gmc.meshHandle);
			if (!model) continue;

			CollidableMesh cm;
			cm.entity = entity;
			cm.meshHandle = gmc.meshHandle;
			
			::Engine::Transform engineTc;
			engineTc.translate = {tc.translate.x, tc.translate.y, tc.translate.z};
			engineTc.rotate = {tc.rotate.x, tc.rotate.y, tc.rotate.z};
			engineTc.scale = {tc.scale.x, tc.scale.y, tc.scale.z};
			cm.world = engineTc.ToMatrix();
			
			const auto& data = model->GetData();
			cm.aabbMin = {FLT_MAX, FLT_MAX, FLT_MAX};
			cm.aabbMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
			for (int k = 0; k < 8; ++k) {
				::Engine::Vector3 p = {(k & 1) ? data.max.x : data.min.x, (k & 2) ? data.max.y : data.min.y, (k & 4) ? data.max.z : data.min.z};
				p = ::Engine::TransformCoord(p, cm.world);
				cm.aabbMin.x = std::min(cm.aabbMin.x, p.x); cm.aabbMin.y = std::min(cm.aabbMin.y, p.y); cm.aabbMin.z = std::min(cm.aabbMin.z, p.z);
				cm.aabbMax.x = std::max(cm.aabbMax.x, p.x); cm.aabbMax.y = std::max(cm.aabbMax.y, p.y); cm.aabbMax.z = std::max(cm.aabbMax.z, p.z);
			}
			m_statics.push_back(cm);
		}

		// --- QuadTreeの構築 ---
		::Engine::PhysicsQuadTree dynamicQT(-4000.0f, -4000.0f, 4000.0f, 4000.0f, 6, 10);
		::Engine::PhysicsQuadTree staticQT(-4000.0f, -4000.0f, 4000.0f, 4000.0f, 6, 10);

		for (size_t i = 0; i < m_dynamics.size(); ++i) {
			dynamicQT.Insert((uint32_t)i, m_dynamics[i].aabbMin.x, m_dynamics[i].aabbMin.z, m_dynamics[i].aabbMax.x, m_dynamics[i].aabbMax.z);
		}
		for (size_t i = 0; i < m_statics.size(); ++i) {
			staticQT.Insert((uint32_t)i, m_statics[i].aabbMin.x, m_statics[i].aabbMin.z, m_statics[i].aabbMax.x, m_statics[i].aabbMax.z);
		}

		// --- Pass 1: Box-Box Collisions (CPU / OBB SAT) ---
		for (int iteration = 0; iteration < 4; ++iteration) {
			for (size_t i = 0; i < m_dynamics.size(); ++i) {
				auto& d1 = m_dynamics[i];
				m_nearbyEntities.clear();
				dynamicQT.Query(d1.aabbMin.x, d1.aabbMin.z, d1.aabbMax.x, d1.aabbMax.z, m_nearbyEntities);

				for (uint32_t j : m_nearbyEntities) {
					if (j <= i) continue;
					auto& d2 = m_dynamics[j];

					float dx = d1.center.x - d2.center.x, dy = d1.center.y - d2.center.y, dz = d1.center.z - d2.center.z;
					float distSq = dx * dx + dy * dy + dz * dz;
					float sphereR1 = std::sqrt(d1.extents.x * d1.extents.x + d1.extents.y * d1.extents.y + d1.extents.z * d1.extents.z);
					float sphereR2 = std::sqrt(d2.extents.x * d2.extents.x + d2.extents.y * d2.extents.y + d2.extents.z * d2.extents.z);
					if (distSq > (sphereR1 + sphereR2) * (sphereR1 + sphereR2)) continue;

					bool isC1 = false, isC2 = false;
					if (auto* tag1 = registry.try_get<TagComponent>(d1.entity)) isC1 = (tag1->tag == TagType::Player || tag1->tag == TagType::Enemy);
					if (auto* tag2 = registry.try_get<TagComponent>(d2.entity)) isC2 = (tag2->tag == TagType::Player || tag2->tag == TagType::Enemy);

					DirectX::XMVECTOR aAxes[3] = {
						DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&d1.axes[0])),
						DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&d1.axes[1])),
						DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&d1.axes[2]))
					};
					DirectX::XMVECTOR bAxes[3] = {
						DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&d2.axes[0])),
						DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&d2.axes[1])),
						DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&d2.axes[2]))
					};

					DirectX::XMVECTOR testAxes[15];
					for (int k = 0; k < 3; ++k) testAxes[k] = aAxes[k];
					for (int k = 0; k < 3; ++k) testAxes[k + 3] = bAxes[k];
					for (int k = 0; k < 3; ++k) {
						for (int l = 0; l < 3; ++l) {
							testAxes[6 + k * 3 + l] = DirectX::XMVector3Cross(aAxes[k], bAxes[l]);
						}
					}

					float minOverlap = FLT_MAX;
					::Engine::Vector3 mtv = {0, 0, 0};
					bool collision = true;
					DirectX::XMVECTOR relPos = DirectX::XMVectorSet(dx, dy, dz, 0.0f);

					for (int k = 0; k < 15; ++k) {
						DirectX::XMVECTOR L = testAxes[k];
						float lenSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(L));
						if (lenSq < 0.001f) continue;
						L = DirectX::XMVectorScale(L, 1.0f / std::sqrt(lenSq));

						float rA = 0, rB = 0;
						for (int m = 0; m < 3; ++m) {
							rA += std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(aAxes[m], L))) * (m == 0 ? d1.extents.x : (m == 1 ? d1.extents.y : d1.extents.z));
							rB += std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(bAxes[m], L))) * (m == 0 ? d2.extents.x : (m == 1 ? d2.extents.y : d2.extents.z));
						}

						float overlap = rA + rB - std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(relPos, L)));
						if (overlap <= 0.0f) { collision = false; break; }
						
						float weight = 1.0f;
						if (isC1 && isC2 && std::abs(DirectX::XMVectorGetY(L)) > 0.5f) weight = 10.0f;

						if (overlap / weight < minOverlap) {
							minOverlap = overlap;
							DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&mtv), L);
							if (DirectX::XMVectorGetX(DirectX::XMVector3Dot(relPos, L)) < 0) {
								mtv.x = -mtv.x; mtv.y = -mtv.y; mtv.z = -mtv.z;
							}
						}
					}

					if (collision) {
						auto& rb1 = registry.get<RigidbodyComponent>(d1.entity);
						auto& rb2 = registry.get<RigidbodyComponent>(d2.entity);
						float move1 = rb1.isKinematic ? 0.0f : (rb2.isKinematic ? 1.0f : 0.5f);
						float move2 = rb2.isKinematic ? 0.0f : (rb1.isKinematic ? 1.0f : 0.5f);

						if (isC1 && isC2) {
							mtv.y = 0;
							float xzLen = std::sqrt(mtv.x * mtv.x + mtv.z * mtv.z);
							if (xzLen > 0.001f) { mtv.x /= xzLen; mtv.z /= xzLen; }
						}

						if (move1 > 0) {
							auto& tc1 = registry.get<TransformComponent>(d1.entity);
							tc1.translate.x += mtv.x * minOverlap * move1; tc1.translate.y += mtv.y * minOverlap * move1; tc1.translate.z += mtv.z * minOverlap * move1;
							d1.center.x += mtv.x * minOverlap * move1; d1.center.y += mtv.y * minOverlap * move1; d1.center.z += mtv.z * minOverlap * move1;
						}
						if (move2 > 0) {
							auto& tc2 = registry.get<TransformComponent>(d2.entity);
							tc2.translate.x -= mtv.x * minOverlap * move2; tc2.translate.y -= mtv.y * minOverlap * move2; tc2.translate.z -= mtv.z * minOverlap * move2;
							d2.center.x -= mtv.x * minOverlap * move2; d2.center.y -= mtv.y * minOverlap * move2; d2.center.z -= mtv.z * minOverlap * move2;
						}

						DirectX::XMVECTOR v1 = rb1.isKinematic ? DirectX::XMVectorZero() : DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&rb1.velocity));
						DirectX::XMVECTOR v2 = rb2.isKinematic ? DirectX::XMVectorZero() : DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&rb2.velocity));
						DirectX::XMVECTOR n = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&mtv));
						float relVel = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVectorSubtract(v1, v2), n));
						if (relVel < 0) {
							DirectX::XMVECTOR impulse = DirectX::XMVectorScale(n, -relVel * 0.5f);
							if (!rb1.isKinematic) DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&rb1.velocity), DirectX::XMVectorAdd(v1, impulse));
							if (!rb2.isKinematic) DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&rb2.velocity), DirectX::XMVectorSubtract(v2, impulse));
						}
					}
				}
			}
		}

		// --- Pass 2: GPU Mesh Collision Batched Requests ---
		m_pendingGpuRequests.clear();
		uint32_t nextResultIdx = 0;

		if (ctx.renderer && !m_dynamics.empty() && !m_statics.empty()) {
			ctx.renderer->BeginCollisionCheck(1024);

			for (auto& d : m_dynamics) {
				auto& tc = registry.get<TransformComponent>(d.entity);
				auto& bc = registry.get<BoxColliderComponent>(d.entity);
				
				::Engine::Transform dynTransform;
				dynTransform.translate = {tc.translate.x, tc.translate.y, tc.translate.z};
				dynTransform.rotate = {tc.rotate.x, tc.rotate.y, tc.rotate.z};
				dynTransform.scale = {tc.scale.x, tc.scale.y, tc.scale.z};
				
				m_nearbyEntities.clear();
				staticQT.Query(d.aabbMin.x, d.aabbMin.z, d.aabbMax.x, d.aabbMax.z, m_nearbyEntities);

				for (uint32_t sIdx : m_nearbyEntities) {
					auto& s = m_statics[sIdx];
					if (d.aabbMax.y < s.aabbMin.y || d.aabbMin.y > s.aabbMax.y ||
					    d.aabbMax.x < s.aabbMin.x || d.aabbMin.x > s.aabbMax.x ||
					    d.aabbMax.z < s.aabbMin.z || d.aabbMin.z > s.aabbMax.z) continue;

					if (nextResultIdx >= 1024) break;

					auto& sTc = registry.get<TransformComponent>(s.entity);
					::Engine::Transform staticTransform;
					staticTransform.translate = {sTc.translate.x, sTc.translate.y, sTc.translate.z};
					staticTransform.rotate = {sTc.rotate.x, sTc.rotate.y, sTc.rotate.z};
					staticTransform.scale = {sTc.scale.x, sTc.scale.y, sTc.scale.z};

					uint32_t rIdx = nextResultIdx++;
					ctx.renderer->DispatchCollision(0, s.meshHandle, dynTransform, bc, staticTransform, rIdx);
					m_pendingGpuRequests.push_back({d.entity, rIdx});
				}
				if (nextResultIdx >= 1024) break;
			}

			ctx.renderer->EndCollisionCheck();

			// --- Pass 3: Resolve GPU Results ---
			for (const auto& req : m_pendingGpuRequests) {
				ContactInfo ci{};
				if (ctx.renderer->GetCollisionResult(req.resultIdx, ci)) {
					auto& tc = registry.get<TransformComponent>(req.entity);
					if (registry.all_of<RigidbodyComponent>(req.entity)) {
						auto& rb = registry.get<RigidbodyComponent>(req.entity);
						if (ci.depth > 0.0f && !rb.isKinematic) {
							tc.translate.x += ci.normal.x * ci.depth;
							tc.translate.y += ci.normal.y * ci.depth;
							tc.translate.z += ci.normal.z * ci.depth;
						}
						DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&rb.velocity));
						DirectX::XMVECTOR n = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&ci.normal));
						float dotVN = DirectX::XMVectorGetX(DirectX::XMVector3Dot(vel, n));
						if (dotVN < 0 && !rb.isKinematic) {
							vel = DirectX::XMVectorSubtract(vel, DirectX::XMVectorScale(n, dotVN));
							DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&rb.velocity), vel);
						}
					}
				}
			}
		}
	}

	void Reset(entt::registry& registry) override {
		auto view = registry.view<RigidbodyComponent>();
		for (auto entity : view) {
			auto& rb = registry.get<RigidbodyComponent>(entity);
			rb.velocity = {0, 0, 0};
		}
	}

private:
	struct CollidableBox {
		entt::entity entity;
		::Engine::Vector3 axes[3], center, extents;
		::Engine::Vector3 aabbMin, aabbMax;
	};
	struct CollidableMesh {
		entt::entity entity = entt::null;
		uint32_t meshHandle = 0;
		::Engine::Vector3 aabbMin = { 0.0f, 0.0f, 0.0f };
		::Engine::Vector3 aabbMax = { 0.0f, 0.0f, 0.0f };
		::Engine::Matrix4x4 world = ::Engine::Matrix4x4::Identity();
	};
	struct GpuRequest {
		entt::entity entity;
		uint32_t resultIdx;
	};

	std::vector<CollidableBox> m_dynamics;
	std::vector<CollidableMesh> m_statics;
	std::vector<GpuRequest> m_pendingGpuRequests;
	std::vector<uint32_t> m_nearbyEntities;

	static void GetObbAxes(const ::Engine::Matrix4x4& mat, const BoxColliderComponent& cb,
		::Engine::Vector3 axes[3], ::Engine::Vector3& center, ::Engine::Vector3& extents) {
		DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&mat));
		DirectX::XMVECTOR c = DirectX::XMVector3TransformCoord(
			DirectX::XMVectorSet(cb.center.x, cb.center.y, cb.center.z, 1.0f), worldMat);
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&center), c);

		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&axes[0]), DirectX::XMVector3Normalize(worldMat.r[0]));
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&axes[1]), DirectX::XMVector3Normalize(worldMat.r[1]));
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&axes[2]), DirectX::XMVector3Normalize(worldMat.r[2]));

		DirectX::XMVECTOR aScale, aRot, aTrans;
		DirectX::XMMatrixDecompose(&aScale, &aRot, &aTrans, worldMat);

		extents.x = cb.size.x * 0.5f * std::abs(DirectX::XMVectorGetX(aScale));
		extents.y = cb.size.y * 0.5f * std::abs(DirectX::XMVectorGetY(aScale));
		extents.z = cb.size.z * 0.5f * std::abs(DirectX::XMVectorGetZ(aScale));
	}
};

} // namespace Game
