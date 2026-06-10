#pragma once
// ==================================================
// SceneManager.h
//   ・シーン登録（名前→Factory）
//   ・現在シーンの保持/切替/更新/描画
//   ・即時切替と「次フレーム切替（リクエスト）」に対応
//   ・デバッグ/自動起動用ユーティリティ追加
// ==================================================
#include "IScene.h"
#include "WindowDX.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

class SceneManager {
public:
	SceneManager() { instance_ = this; }
	~SceneManager() { if (instance_ == this) instance_ = nullptr; }

public:
	static SceneManager* GetInstance();

	using Factory = std::function<std::unique_ptr<IScene>()>;

public:
	void Register(const std::string& name, Factory factory);

	bool Change(const std::string& name, const SceneParameters& params = {});
	void RequestChange(const std::string& name, const SceneParameters& params = {});

	void Update();
	void Draw();

	IScene* Current() const { return current_.get(); }
	const std::string& CurrentName() const { return currentName_; }

	void SetDX(WindowDX* dx) { dx_ = dx; }

	void Clear();

	// ---- 追加：原因調査＆自動起動用 ----
	bool Has(const std::string& name) const;
	std::string FirstRegisteredName() const; // 登録済みの先頭（無ければ空）
	std::vector<std::string> RegisteredNames() const;

private:
	std::unordered_map<std::string, Factory> factories_;
	std::unique_ptr<IScene> current_;
	std::string currentName_;
	std::string pendingNext_;
	SceneParameters pendingParams_;
	WindowDX* dx_ = nullptr;

	static SceneManager* instance_;
};

} // namespace Engine
