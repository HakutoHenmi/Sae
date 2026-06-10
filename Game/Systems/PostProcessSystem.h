#pragma once
#include "ISystem.h"
#include "../../Engine/Renderer.h"
#include "../ObjectTypes.h"
#include "PlayerActionSystem.h"
#include <algorithm>
#include <cmath>

namespace Game {

class PostProcessSystem : public ISystem {
public:
    void Update(entt::registry& registry, GameContext& ctx) override {
        auto* renderer = ctx.renderer;
        if (!renderer) return;

        // --- 0. 和風テクスチャのロードと設定 (初回のみ) ---
        static bool texturesLoaded = false;
        if (!texturesLoaded) {
            auto paper = renderer->LoadTexture2D("Resources/Textures/paper.png");
            auto vignette = renderer->LoadTexture2D("Resources/Textures/vignette.png");
            renderer->SetSumiETextures(paper, vignette);
            texturesLoaded = true;
        }

        // --- 1. ベースパラメータの設定 (明るさとクリアさを重視) ---
        Engine::Renderer::PostProcessParams target;
        target.noiseStrength = 0.6f; // 紙の凹凸による歪みと質感の強さ
        target.vignette = 0.0f;      // デフォルトではダメージビネットはゼロ
        target.chromaShift = 0.6f;   // 線の太さ
        target.distortion = 0.0f;
        target.san = 0.0f;
        target.scanline = 0.02f;      // 極めて薄い階調化 (常時)

        bool isLowHealth = false;
        bool isDead = false;
        bool hasLockedTarget = false;

        auto playerView = registry.view<TagComponent, HealthComponent, PlayerInputComponent>();
        entt::entity playerEntity = entt::null;
        playerView.each([&](entt::entity entity, const TagComponent& tagComp, const HealthComponent&, const PlayerInputComponent&) {
            if (tagComp.tag == TagType::Player) {
                playerEntity = entity;
            }
        });

        if (playerEntity != entt::null) {
            auto& hc = playerView.get<HealthComponent>(playerEntity);
            auto& pi = playerView.get<PlayerInputComponent>(playerEntity);

            isDead = hc.isDead;
            float hpRate = hc.hp / (hc.maxHp > 0 ? hc.maxHp : 1.0f);
            isLowHealth = (hpRate < 0.3f);

            if (hc.hitFlashTimer > 0.0f) hitPulseTimer_ = 0.4f;
            hasLockedTarget = (pi.lockedEnemy != entt::null && registry.valid(pi.lockedEnemy));

            if (registry.all_of<PlayerActionComponent>(playerEntity)) {
                auto& pa = registry.get<PlayerActionComponent>(playerEntity);
                
                bool isStrongAttack = (pa.state == PlayerActionState::SlimeHammer);
                
                if (isStrongAttack && pa.stateTimer < 0.1f) {
                    float power = 1.0f;
                    actionPulseTimer_ = std::max(actionPulseTimer_, power); 
                }

                // ヒットインパクト
                if (pa.hitStopTimer > 0.0f) {
                    hitStopPulse_ = std::lerp(hitStopPulse_, 1.5f, std::clamp(ctx.dt * 20.0f, 0.0f, 1.0f));
                    inkWashFade_ = std::lerp(inkWashFade_, 0.5f, std::clamp(ctx.dt * 15.0f, 0.0f, 1.0f));
                } else {
                    hitStopPulse_ = std::lerp(hitStopPulse_, 0.0f, std::clamp(ctx.dt * 12.0f, 0.0f, 1.0f));
                    inkWashFade_ = std::lerp(inkWashFade_, 0.0f, std::clamp(ctx.dt * 8.0f, 0.0f, 1.0f));
                }
            }

            // 速度ブラー
            if (registry.all_of<TransformComponent>(playerEntity)) {
                auto& tc = registry.get<TransformComponent>(playerEntity);
                static DirectX::XMFLOAT3 lastPos = tc.translate;
                float dx = tc.translate.x - lastPos.x;
                float dy = tc.translate.y - lastPos.y;
                float dz = tc.translate.z - lastPos.z;
                float speed = std::sqrt(dx*dx + dy*dy + dz*dz) / (ctx.dt > 0 ? ctx.dt : 0.016f);
                lastPos = tc.translate;

                float speedThreshold = 8.0f;
                float speedRate = std::clamp((speed - speedThreshold) / 12.0f, 0.0f, 1.2f);
                speedEffectIntensity_ = std::lerp(speedEffectIntensity_, speedRate, std::clamp(ctx.dt * 10.0f, 0.0f, 1.0f));
            }
        }

        // --- 2. パラメータ合成 ---
        if (isDead) {
            target.scanline = 1.0f; 
            target.vignette = 0.4f;
        } 
        else {
            if (hasLockedTarget) target.chromaShift = 0.45f; 

            float radialEffect = speedEffectIntensity_;
            if (actionPulseTimer_ > 0.0f) {
                radialEffect = std::max(radialEffect, actionPulseTimer_ * 1.2f);
                actionPulseTimer_ -= ctx.dt * 2.0f;
            }
            if (hitPulseTimer_ > 0.0f) {
                radialEffect = std::max(radialEffect, hitPulseTimer_ * 2.0f);
                target.vignette = std::max(target.vignette, hitPulseTimer_ * 1.5f); // 被弾時に赤くする
                hitPulseTimer_ -= ctx.dt;
            }
            radialEffect = std::max(radialEffect, hitStopPulse_);

            target.distortion = radialEffect;
            target.scanline = std::max(target.scanline, inkWashFade_);

            if (isLowHealth) {
                target.scanline = std::max(target.scanline, 0.3f);
                target.vignette = 0.2f;
            }
        }

        auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };
        float lerpSpeed = std::clamp(ctx.dt * 10.0f, 0.0f, 1.0f);

        currentParams_.noiseStrength = Lerp(currentParams_.noiseStrength, target.noiseStrength, lerpSpeed);
        currentParams_.distortion = Lerp(currentParams_.distortion, target.distortion, lerpSpeed);
        currentParams_.chromaShift = Lerp(currentParams_.chromaShift, target.chromaShift, lerpSpeed);
        currentParams_.vignette = Lerp(currentParams_.vignette, target.vignette, lerpSpeed);
        currentParams_.scanline = Lerp(currentParams_.scanline, target.scanline, lerpSpeed);

        currentParams_.time = renderer->GetPostProcessParams().time;
        renderer->SetPostProcessParams(currentParams_);
        if (isDead) {
            renderer->SetPostEffect("Smoothing");
        } else if (isLowHealth) {
            renderer->SetPostEffect("GaussianFilter");
        } else {
            renderer->SetPostEffect("Rich");
        }
    }

    void Draw(entt::registry& /*registry*/, GameContext& /*ctx*/) override {}

    void Reset(entt::registry& /*registry*/) override {
        currentParams_ = Engine::Renderer::PostProcessParams();
        currentParams_.vignette = 0.0f;
        currentParams_.noiseStrength = 0.6f;
        hitPulseTimer_ = 0.0f;
        actionPulseTimer_ = 0.0f;
        hitStopPulse_ = 0.0f;
        inkWashFade_ = 0.0f;
        speedEffectIntensity_ = 0.0f;
    }

private:
    Engine::Renderer::PostProcessParams currentParams_;
    float hitPulseTimer_ = 0.0f;
    float actionPulseTimer_ = 0.0f;
    float hitStopPulse_ = 0.0f;
    float inkWashFade_ = 0.0f;
    float speedEffectIntensity_ = 0.0f;
};

} // namespace Game
