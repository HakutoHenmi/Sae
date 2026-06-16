#include "App.h"
#include "MainScene.h"
#include <Windows.h>
#include <memory>
#include <winrt/Windows.Foundation.h>

int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int cmdShow) {
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	winrt::init_apartment(winrt::apartment_type::multi_threaded);

	// カレントディレクトリをexeの場所に設定
	{
		wchar_t exePath[32768];
		DWORD length = GetModuleFileNameW(nullptr, exePath, 32768);
		if (length > 0 && length < 32768) {
			exePath[length] = L'\0';
			wchar_t* lastSlash = wcsrchr(exePath, L'\\');
			if (lastSlash)
				*lastSlash = L'\0';

			// プロジェクトルートを探す (neo_Engine ディレクトリ)
			wchar_t projectPath[32768] = {};
			wcscpy_s(projectPath, exePath);
			wcscat_s(projectPath, L"\\..\\..\\..\\neo_Engine");

			DWORD attr = GetFileAttributesW(projectPath);
			if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
				SetCurrentDirectoryW(projectPath);
			} else {
				SetCurrentDirectoryW(exePath);
			}
		}
	}

	Engine::App app;

	app.SetSceneRegistrar([](Engine::SceneManager& sm, Engine::WindowDX& dx) {
		(void)dx;
		sm.Register("Main", []() -> std::unique_ptr<Engine::IScene> { return std::unique_ptr<Engine::IScene>(new Game::MainScene()); });
	});

	// Default Scene
	app.SetInitialSceneKey("Main");

	if (!app.Initialize(hInst, cmdShow)) {
		return -1;
	}

	app.Run();
	app.Shutdown();
	return 0;
}
