#pragma once
#include "../ObjectTypes.h"
#include "../../Engine/Camera.h"
#include "../../Engine/Renderer.h"
#include "../../Engine/Input.h"
#include "../../Engine/EventSystem.h" // ★追加: イベントシステム
#include <vector>
#include "../../externals/entt/entt.hpp"

namespace Game {

// 各Systemに渡す共有コンテキスト
struct GameContext {
	float dt = 0.0f;
	Engine::Camera* camera = nullptr;
	Engine::Renderer* renderer = nullptr;
	class MainScene* scene = nullptr; // ★追加
	Engine::Input* input = nullptr;
	Engine::EventSystem* eventSystem = nullptr; // ★追加: スクリプト間通信用
	bool isPlaying = false;
	entt::registry* pendingSpawns = nullptr; // SpawnObject等の遅延追加用

	// ★追加: 座標系補正用 (エディターGameビュー等での相対座標)
	bool useOverrideMouse = false;
	float overrideMouseX = 0.0f;
	float overrideMouseY = 0.0f;
	DirectX::XMFLOAT2 viewportOffset = { 0, 0 };
	DirectX::XMFLOAT2 viewportSize = { 0, 0 };
};

// System基底インターフェース
class ISystem {
public:
	virtual ~ISystem() = default;
	virtual void Update(entt::registry& registry, GameContext& ctx) = 0;
	virtual void Draw(entt::registry& /*registry*/, GameContext& /*ctx*/) {} // 描画処理用
	virtual void DrawUI(entt::registry& /*registry*/, GameContext& /*ctx*/) {} // ImGui等のUI処理用
	virtual void Reset(entt::registry& /*registry*/) {} // Play開始時のリセット
};

} // namespace Game
