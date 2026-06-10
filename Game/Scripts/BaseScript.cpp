#include "BaseScript.h"
#include "ObjectTypes.h"
#include "Scenes/MainScene.h"
#include "ScriptEngine.h"

#ifdef USE_IMGUI
#include "../../externals/imgui/imgui.h"
#endif

#include <sstream>

namespace Game {

void BaseScript::Start(entt::entity entity, MainScene* scene) {
	// 初期化処理をここに記述
	(void)entity;
	(void)scene;
}

void BaseScript::Update(entt::entity entity, MainScene* scene, float dt) {
	// 毎フレームの更新処理をここに記述
	(void)entity;
	(void)scene;
	(void)dt;
}

void BaseScript::OnDestroy(entt::entity /*entity*/, MainScene* /*scene*/) {}

void BaseScript::OnEditorUI() {
#if defined(USE_IMGUI) && !defined(NDEBUG)
	// エディタ上のパラメータ表示
	// 例: ImGui::DragFloat("Speed", &speed_, 0.1f);
#endif
}

std::string BaseScript::SerializeParameters() {
	return "";
}

void BaseScript::DeserializeParameters(const std::string& data) {
	(void)data;
}

REGISTER_SCRIPT(BaseScript);

} // namespace Game