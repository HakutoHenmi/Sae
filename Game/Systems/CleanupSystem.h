#pragma once
#include "ISystem.h"
#include <algorithm>
#include <cmath>

namespace Game {

class CleanupSystem : public ISystem {
public:
	void Update(entt::registry& registry, GameContext& ctx) override {
		if (!ctx.isPlaying) return;

		// 射程外の弾消去
		auto bulletView = registry.view<TagComponent, TransformComponent, HealthComponent>();
		for (auto entity : bulletView) {
			auto& tag = bulletView.get<TagComponent>(entity);
			if (tag.tag != TagType::Bullet) continue;

			auto& tc = bulletView.get<TransformComponent>(entity);
			float distSq = tc.translate.x * tc.translate.x +
			               tc.translate.y * tc.translate.y +
			               tc.translate.z * tc.translate.z;
			if (distSq > 10000.0f) {
				auto& hc = bulletView.get<HealthComponent>(entity);
				hc.isDead = true;
			}
		}

		// dead のエンティティを破棄
		std::vector<entt::entity> toDestroy;
		
		// ★追加: 自動削除コンポーネントの処理
		auto autoView = registry.view<AutoDestroyComponent>();
		for (auto entity : autoView) {
			auto& adc = registry.get<AutoDestroyComponent>(entity);
			adc.timer -= ctx.dt;
			if (adc.timer <= 0.0f) {
				toDestroy.push_back(entity);
			}
		}

		auto healthView = registry.view<HealthComponent>();
		for (auto entity : healthView) {
			auto& hc = registry.get<HealthComponent>(entity);
			if (hc.isDead && hc.enabled) {
				// Player は破棄しない
				if (registry.all_of<TagComponent>(entity)) {
					auto& tag = registry.get<TagComponent>(entity);
					if (tag.tag == TagType::Player) continue;
				}
				toDestroy.push_back(entity);
			}
		}
		for (auto e : toDestroy) {
			if (registry.valid(e)) {
				registry.destroy(e);
			}
		}
	}
};

} // namespace Game
