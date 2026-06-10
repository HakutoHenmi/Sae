#include "App.h"
#include <Windows.h>
#include <chrono>
#include "JobSystem.h"
#include "Time/TimeManager.h" // ★追加
#include "SharedMemoryProfiler.h"

namespace Engine {

bool App::Initialize(HINSTANCE hInst, int cmdShow) {
	sceneManager_.SetDX(&dx_);
	if (!dx_.Initialize(hInst, cmdShow, hwnd_))
		return false;

	// Job Systemの初期化
	JobSystem::Initialize();

	// 共有メモリプロファイラの初期化
	SharedMemoryProfiler::GetInstance().Initialize();

	if (!renderer_.Initialize(&dx_))
		return false;

	input_.Initialize(hInst, hwnd_);
	camera_.Initialize();
	audio_.Initialize();

#ifdef USE_IMGUI
	if (!imgui_.Initialize(hwnd_, dx_, dx_.SRV(), dx_.SRV_CPU(0), dx_.SRV_GPU(0), 18.0f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf")) {
		return false;
	}
#endif

	if (registrar_) {
		registrar_(sceneManager_, dx_);
	}

	if (!initialSceneKey_.empty()) {
		sceneManager_.Change(initialSceneKey_);
	} else if (sceneManager_.Has("FPS")) {
		sceneManager_.Change("FPS");
	} else {
		const std::string first = sceneManager_.FirstRegisteredName();
		if (!first.empty())
			sceneManager_.Change(first);
	}

	return true;
}

void App::Run() {
	MSG msg{};
	bool running = true;

	auto prevTime = std::chrono::high_resolution_clock::now();
	int frameCount = 0;
	float timeElapsed = 0.0f;

	while (running) {
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (!running)
			break;

		input_.Update();

		// ★追加: フルスクリーン切り替え (F11 or Alt+Enter)
		if (input_.Trigger(DIK_F11) || (input_.Down(DIK_LALT) && input_.Trigger(DIK_RETURN))) {
			dx_.ToggleFullscreen();
		}

		// フレームタイムの計算
		auto currentTime = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration<float>(currentTime - prevTime).count();
		prevTime = currentTime;

		// FPSの計算
		frameCount++;
		timeElapsed += dt;
		if (timeElapsed >= 1.0f) {
			SharedMemoryProfiler::GetInstance().SetFPS((float)frameCount / timeElapsed);
			frameCount = 0;
			timeElapsed = 0.0f;
		}

		// ★追加: TimeManagerの更新
		TimeManager::GetInstance().Update(dt);

		// プロファイラにフレームタイムをセット
		SharedMemoryProfiler::GetInstance().SetDeltaTime(dt);

		dx_.BeginFrame();
		const float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f}; // 背景透過のため完全透明に
		renderer_.BeginFrame(clearColor);

#ifdef USE_IMGUI
		imgui_.NewFrame(dx_);
#endif

		sceneManager_.Update();
		sceneManager_.Draw();

		renderer_.EndFrame();

#ifdef USE_IMGUI
 		Engine::IScene* currentScene = sceneManager_.Current();
 		if (currentScene) {
#ifndef NDEBUG
 			currentScene->DrawEditor();
#endif
			currentScene->DrawUI(); // リバースでもエディタでも共通で描画
 		}

		imgui_.Render(dx_);
#endif
		dx_.EndFrame();

		// ★追加: 詳細な統計データをプロファイラへ送る
		auto* renderer = Renderer::GetInstance();
		if (renderer) {
			SharedMemoryProfiler::GetInstance().SetDrawCalls(renderer->GetDrawCallCount());
			SharedMemoryProfiler::GetInstance().SetParticleCount(renderer->GetParticleCount());
			
			// 有効なライトの数を数える
			uint32_t activeLights = 0;
			auto lcb = renderer->GetLightCB();
			for (int i = 0; i < Renderer::kMaxDirLights; ++i) if (lcb.dirLights[i].enabled) activeLights++;
			for (int i = 0; i < Renderer::kMaxPointLights; ++i) if (lcb.pointLights[i].enabled) activeLights++;
			for (int i = 0; i < Renderer::kMaxSpotLights; ++i) if (lcb.spotLights[i].enabled) activeLights++;
			for (int i = 0; i < Renderer::kMaxAreaLights; ++i) if (lcb.areaLights[i].enabled) activeLights++;
			SharedMemoryProfiler::GetInstance().SetLightCount(activeLights);

			Vector3 pPos = renderer->GetPlayerPos();
			SharedMemoryProfiler::GetInstance().SetPlayerPos(pPos.x, pPos.y, pPos.z);
		}

		// プロファイラのデータを共有メモリに書き込む
		SharedMemoryProfiler::GetInstance().CommitFrame();
	}
}

void App::Shutdown() {
	SharedMemoryProfiler::GetInstance().Shutdown();
	JobSystem::Shutdown();
#ifdef USE_IMGUI
	imgui_.Shutdown();
#endif
	audio_.Shutdown();
	input_.Shutdown();

	// ★修正: RendererとSceneをDeviceより先に解放する
	sceneManager_.Clear();
	renderer_.Shutdown();

	dx_.WaitIdle();
	dx_.Shutdown();
}

void App::BeginFrame_() {}
void App::EndFrame_() {}

} // namespace Engine
