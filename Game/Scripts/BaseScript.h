#pragma once
#include "IScript.h"
#include "Scenes/MainScene.h"

#include "../../externals/entt/entt.hpp"

namespace Game {

// サンプルスクリプト: 新しいスクリプトを作る際のテンプレートとして利用してください
class BaseScript : public IScript {
public:
	void Start(entt::entity entity, MainScene* scene) override;
	void Update(entt::entity entity, MainScene* scene, float dt) override;
	void OnDestroy(entt::entity entity, MainScene* scene) override;
	void OnEditorUI() override;
	std::string SerializeParameters() override;
	void DeserializeParameters(const std::string& data) override;
};

} // namespace Game