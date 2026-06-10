#pragma once
#include "ISystem.h"
#include "../../Engine/Audio.h"
#include <cmath>

namespace Game {

class AudioSystem : public ISystem {
public:
	void Update(entt::registry& registry, GameContext& ctx) override {
		if (!ctx.isPlaying) return;

		// リスナー位置の検索
		DirectX::XMFLOAT3 listenerPos = ctx.camera ? ctx.camera->Position() : DirectX::XMFLOAT3{0, 0, 0};
		auto listenerView = registry.view<AudioListenerComponent, TransformComponent>();
		for (auto entity : listenerView) {
			auto& al = listenerView.get<AudioListenerComponent>(entity);
			if (al.enabled) {
				auto& tc = listenerView.get<TransformComponent>(entity);
				listenerPos = tc.translate;
				break;
			}
		}

		auto* audio = Engine::Audio::GetInstance();
		if (!audio) return;

		auto sourceView = registry.view<AudioSourceComponent, TransformComponent>();
		for (auto entity : sourceView) {
			auto& as = sourceView.get<AudioSourceComponent>(entity);
			auto& tc = sourceView.get<TransformComponent>(entity);
			if (!as.enabled) continue;

			if (as.playOnStart && !as.isPlaying && as.soundHandle != 0xFFFFFFFF) {
				as.voiceHandle = audio->Play(as.soundHandle, as.loop, as.volume);
				as.isPlaying = true;
			}

			if (as.isPlaying && as.voiceHandle != 0) {
				float finalVol = as.volume;
				if (as.is3D) {
					float dx = tc.translate.x - listenerPos.x;
					float dy = tc.translate.y - listenerPos.y;
					float dz = tc.translate.z - listenerPos.z;
					float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
					if (as.maxDistance > 0.001f) {
						float atten = 1.0f - (dist / as.maxDistance);
						if (atten < 0.0f) atten = 0.0f;
						atten = atten * atten;
						finalVol *= atten;
					}
				}
				
				// ★追加: マスター音量の適用
				if (as.category == AudioCategory::BGM) {
					finalVol *= audio->GetMasterBGMVolume();
				} else {
					finalVol *= audio->GetMasterSEVolume();
				}
				
				audio->SetVolume(as.voiceHandle, finalVol);
			}
		}
	}
};

} // namespace Game
