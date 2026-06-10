#pragma once
#include "../Engine/Input.h"
#include "IScript.h"
#include "ObjectTypes.h"
#include "../../externals/entt/entt.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Game {

class MainScene; // 前方宣言

class ScriptEngine {
public:
	static ScriptEngine* GetInstance();

	void Initialize();
	void Shutdown();

	void Execute(entt::entity entity, MainScene* scene, float dt);

	using ScriptCreator = std::function<std::shared_ptr<IScript>()>;
	void RegisterScript(const std::string& className, ScriptCreator creator);
	std::shared_ptr<IScript> CreateScript(const std::string& className);
	std::vector<std::string> GetRegisteredScriptNames() const {
		std::vector<std::string> names;
		for (auto const& [name, _] : scriptFactory_) names.push_back(name);
		return names;
	}

private:
	ScriptEngine() {}
	~ScriptEngine() {}

	static ScriptEngine* instance_;
	std::map<std::string, ScriptCreator> scriptFactory_;
};

} // namespace Game

// ==============================================================
// ★追加: C++スクリプトを自動登録するためのマクロ
// クラスの .cpp ファイルの一番下に REGISTER_SCRIPT(クラス名); と書くだけで登録されます。
// ==============================================================
#define REGISTER_SCRIPT(ClassName)                                                                                                                                                                     \
	namespace {                                                                                                                                                                                        \
	struct ClassName##_Register {                                                                                                                                                                      \
		ClassName##_Register() {                                                                                                                                                                       \
			Game::ScriptEngine::GetInstance()->RegisterScript(#ClassName, []() { return std::make_shared<ClassName>(); });                                                                             \
		}                                                                                                                                                                                              \
	} ClassName##_register_instance;                                                                                                                                                                   \
	}