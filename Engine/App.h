#pragma once
#include "Audio.h"
#include "Camera.h"
#ifdef USE_IMGUI
#include "ImGuiLayer.h"
#endif
#include "Input.h"
#include "Matrix4x4.h"
#include "Renderer.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "WindowDX.h"

// GameSceneの中身を知るためにインクルードが必要な場合がありますが、
// ここでは前方宣言でも大丈夫です。メンバ変数としては削除します。


#include <Windows.h>
#include <chrono>
#include <dxgidebug.h>
#include <functional>
#include <string>
#include <vector>
#include <wrl.h>
#pragma comment(lib, "dxguid.lib")

namespace Engine {

class App {
public:
	bool Initialize(HINSTANCE hInst, int cmdShow);
	void Run();
	void Shutdown();

	using SceneRegistrar = std::function<void(SceneManager& sm, WindowDX& dx)>;
	void SetSceneRegistrar(SceneRegistrar f) { registrar_ = std::move(f); }
	void SetInitialSceneKey(const std::string& key) { initialSceneKey_ = key; }

private:
	void BeginFrame_();
	void EndFrame_();

private:
	SceneManager sceneManager_;
	HWND hwnd_ = nullptr;
	WindowDX dx_;
	Input input_;
	Camera camera_;
	Audio audio_;
	Renderer renderer_;
#ifdef USE_IMGUI
	ImGuiLayer imgui_;
#endif

	std::chrono::steady_clock::time_point prev_{};

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	SceneRegistrar registrar_{};
	std::string initialSceneKey_{};

	// ★修正: App自身がWorldを持つのはやめる (GameSceneが持つため)
	// Game::World world_;  <-- 削除済み
};

} // namespace Engine