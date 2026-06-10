#pragma once
#include "ISystem.h"
#include "../Math/Spline.h"

namespace Game {

class MotionSystem : public ISystem {
public:
    void Update(entt::registry& registry, GameContext& ctx) override {
        auto view = registry.view<MotionComponent, TransformComponent>();
        for (auto entity : view) {
            auto& motion = view.get<MotionComponent>(entity);
            auto& transform = view.get<TransformComponent>(entity);

            if (motion.clips.find(motion.activeClip) == motion.clips.end()) continue;
            auto& clip = motion.clips[motion.activeClip];

            if (motion.isPlaying && ctx.isPlaying) {
                motion.currentTime += ctx.dt;
                if (clip.loop) {
                    motion.currentTime = fmod(motion.currentTime, clip.totalDuration);
                } else if (motion.currentTime > clip.totalDuration) {
                    motion.currentTime = clip.totalDuration;
                    motion.isPlaying = false;
                }
            }

            // Apply motion to transform
            ApplyMotion(motion, transform);
        }
    }

    void ApplyMotion(const MotionComponent& motion, TransformComponent& transform) {
        auto it = motion.clips.find(motion.activeClip);
        if (it == motion.clips.end()) return;
        const auto& clip = it->second;

        if (clip.keyframes.size() < 2) return;

        float t = motion.currentTime;
        std::vector<DirectX::XMFLOAT3> posPoints;
        for (const auto& kf : clip.keyframes) posPoints.push_back(kf.translate);

        float splineT = (t / clip.totalDuration) * (static_cast<float>(clip.keyframes.size()) - 1.0f);
        DirectX::XMVECTOR pos = Engine::Spline::Interpolate(posPoints, splineT);
        DirectX::XMStoreFloat3(&transform.translate, pos);

        int i = static_cast<int>(splineT);
        int next = (i + 1 < (int)clip.keyframes.size()) ? i + 1 : i;
        float localT = splineT - static_cast<float>(i);

        transform.rotate.x = clip.keyframes[i].rotate.x * (1.0f - localT) + clip.keyframes[next].rotate.x * localT;
        transform.rotate.y = clip.keyframes[i].rotate.y * (1.0f - localT) + clip.keyframes[next].rotate.y * localT;
        transform.rotate.z = clip.keyframes[i].rotate.z * (1.0f - localT) + clip.keyframes[next].rotate.z * localT;

        transform.scale.x = clip.keyframes[i].scale.x * (1.0f - localT) + clip.keyframes[next].scale.x * localT;
        transform.scale.y = clip.keyframes[i].scale.y * (1.0f - localT) + clip.keyframes[next].scale.y * localT;
        transform.scale.z = clip.keyframes[i].scale.z * (1.0f - localT) + clip.keyframes[next].scale.z * localT;
    }

    void Draw(entt::registry& registry, GameContext& ctx) override {
        if (!ctx.isPlaying) {
            auto view = registry.view<MotionComponent, TransformComponent>();
            for (auto entity : view) {
                auto& motion = view.get<MotionComponent>(entity);
                auto it = motion.clips.find(motion.activeClip);
                if (it == motion.clips.end()) continue;
                const auto& clip = it->second;
                if (clip.keyframes.size() < 2) continue;

                // 親のワールド行列を取得
                DirectX::XMMATRIX parentWorld = DirectX::XMMatrixIdentity();
                if (registry.all_of<HierarchyComponent>(entity)) {
                    auto& hc = registry.get<HierarchyComponent>(entity);
                    if (hc.parentId != entt::null && registry.valid(hc.parentId)) {
                        // MainSceneから取得（ISystem::DrawのGameContext.sceneを利用）
                        auto worldMat = ctx.scene->GetWorldMatrix((int)hc.parentId);
                        parentWorld = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&worldMat));
                    }
                }

                const int segments = 50;
                Engine::Vector3 prevP;
                std::vector<DirectX::XMFLOAT3> posPoints;
                for (const auto& kf : clip.keyframes) posPoints.push_back(kf.translate);

                for (int i = 0; i <= segments; ++i) {
                    float t = (static_cast<float>(i) / segments) * (clip.keyframes.size() - 1);
                    DirectX::XMVECTOR pLocal = Engine::Spline::Interpolate(posPoints, t);
                    // ワールド座標に変換
                    DirectX::XMVECTOR pWorld = DirectX::XMVector3TransformCoord(pLocal, parentWorld);
                    
                    Engine::Vector3 currP;
                    DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&currP), pWorld);
                    
                    if (i > 0) ctx.renderer->DrawLine3D(prevP, currP, {1, 1, 0, 1}, true);
                    prevP = currP;
                }

                for (int i = 0; i < (int)clip.keyframes.size(); ++i) {
                    Engine::Vector4 color = (i == motion.selectedKeyframe) ? Engine::Vector4{1, 0, 0, 1} : Engine::Vector4{0, 1, 1, 1};
                    DirectX::XMVECTOR pLocal = DirectX::XMLoadFloat3(&clip.keyframes[i].translate);
                    DirectX::XMVECTOR pWorld = DirectX::XMVector3TransformCoord(pLocal, parentWorld);
                    Engine::Vector3 p;
                    DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&p), pWorld);

                    float s = 0.2f;
                    ctx.renderer->DrawLine3D({p.x - s, p.y, p.z}, {p.x + s, p.y, p.z}, color, true);
                    ctx.renderer->DrawLine3D({p.x, p.y - s, p.z}, {p.x, p.y + s, p.z}, color, true);
                    ctx.renderer->DrawLine3D({p.x, p.y, p.z - s}, {p.x, p.y, p.z + s}, color, true);
                }
            }
        }
    }

    void Reset(entt::registry& registry) override {
        auto view = registry.view<MotionComponent>();
        for (auto entity : view) {
            auto& motion = registry.get<MotionComponent>(entity);
            if (motion.enabled) {
                motion.isPlaying = true;
                motion.currentTime = 0.0f;
            }
        }
    }
};

} // namespace Game
