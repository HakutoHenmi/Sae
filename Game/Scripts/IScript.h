#pragma once
#include <string>
#include <functional>

#include "../../externals/entt/entt.hpp"

namespace Game {

struct SceneObject;
class MainScene;

class IScript {
public:
	virtual ~IScript() = default;

	// スクリプトの初期化（アタッチ時やシーン開始時に1回呼ばれる）
	virtual void Start(entt::entity /*entity*/, MainScene* /*scene*/) {}
	
	// 毎フレーム呼ばれる更新処理
	virtual void Update(entt::entity /*entity*/, MainScene* /*scene*/, float /*dt*/) {}
	
	// UIクリック時に呼ばれる
	virtual void OnClick(entt::entity /*entity*/, MainScene* /*scene*/, const std::string& /*callbackName*/) {}
	
	// オブジェクト破棄時やスクリプトが外れた時に呼ばれる
	virtual void OnDestroy(entt::entity /*entity*/, MainScene* /*scene*/) {}

	// エディターUI描画用
	virtual void OnEditorUI() {}

	// パラメーターの個別保存・読み込み用 (エディター用)
	virtual std::string SerializeParameters() { return ""; }
	virtual void DeserializeParameters(const std::string& /*data*/) {}

	// === 便利機能 (ショートカット) ===

	// イベント発行 (EventSystem連携)
	void Emit(MainScene* scene, const std::string& eventName, float value = 0.0f);
	void EmitString(MainScene* scene, const std::string& eventName, const std::string& value = "");
	void EmitVoid(MainScene* scene, const std::string& eventName);

	// イベント購読 (EventSystem連携)
	// ※登録したコールバックの解除はシーン切り替え時に一括で行われるのが基本です
	void Subscribe(MainScene* scene, const std::string& eventName, std::function<void(float)> callback);
	void SubscribeString(MainScene* scene, const std::string& eventName, std::function<void(const std::string&)> callback);
	void SubscribeVoid(MainScene* scene, const std::string& eventName, std::function<void()> callback);

	// 共有変数の取得・設定 (VariableComponent連携)
	void SetVar(entt::entity entity, MainScene* scene, const std::string& key, float value);
	void SetVarString(entt::entity entity, MainScene* scene, const std::string& key, const std::string& value);
	float GetVar(entt::entity entity, MainScene* scene, const std::string& key, float defaultVal = 0.0f);
	std::string GetVarString(entt::entity entity, MainScene* scene, const std::string& key, const std::string& defaultVal = "");
};

} // namespace Game
