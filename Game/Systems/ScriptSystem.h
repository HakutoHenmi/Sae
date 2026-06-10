#pragma once
#include "../Scripts/ScriptEngine.h"
#include "ISystem.h"
#include "../../Engine/JobSystem.h"

namespace Game {

class MainScene; // 前方宣言

class ScriptSystem : public ISystem {
public:
	void SetScene(MainScene* scene) { scene_ = scene; }

	void Update(entt::registry& registry, GameContext& ctx) override {
		if (!ctx.isPlaying)
			return;

		auto* scriptEngine = ScriptEngine::GetInstance();

		auto view = registry.view<ScriptComponent>();
		std::vector<entt::entity> entities(view.begin(), view.end());
		if (entities.empty()) return;

		for (auto entity : entities) {
			if (!registry.valid(entity)) continue;
			auto& sc = registry.get<ScriptComponent>(entity);
			if (sc.enabled && !sc.scripts.empty()) {
				scriptEngine->Execute(entity, scene_, ctx.dt);
			}
		}
	}

	void Reset(entt::registry& registry) override {
		auto view = registry.view<ScriptComponent>();
		for (auto entity : view) {
			auto& sc = registry.get<ScriptComponent>(entity);
			for (auto& entry : sc.scripts) {
				entry.instance = nullptr;
				entry.isStarted = false;
			}
		}
	}

private:
	MainScene* scene_ = nullptr;
};

} // namespace Game
