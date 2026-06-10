#pragma once
#include "Renderer.h"
#include "Audio.h"
#include "../ObjectTypes.h"
#include <vector>
#include <functional>
#include <string>
#include <deque>
#include <DirectXMath.h>

struct ImVec2;

namespace Game {

class MainScene;

// Undo/Redo用のコマンド
struct UndoCommand {
	std::string description;
	std::function<void()> undo;
	std::function<void()> redo;
};

// ギズモ操作モード
enum class GizmoMode {
	Translate,
	Rotate,
	Scale,
};

// ★ Consoleログエントリ
enum class LogLevel { Info, Warning, Error };
struct LogEntry {
	LogLevel level;
	std::string message;
	float time;
};

class EditorUI {
public:
	static void Show(Engine::Renderer* renderer, MainScene* gameScene);

	// Undo/Redo API
	static void PushUndo(const UndoCommand& cmd);
	static void Undo();
	static void Redo();

	// レンダリングサポート用
	static void ScreenToWorldRay(float screenX, float screenY, float imageW, float imageH, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj, DirectX::XMVECTOR& outOrig, DirectX::XMVECTOR& outDir);

	// ★ Console API
	static void Log(const std::string& msg);
	static void LogWarning(const std::string& msg);
	static void LogError(const std::string& msg);
	
	static ImVec2 GetGameImageMin();
	static ImVec2 GetGameImageMax();

	// ★ シーン保存/読み込み
	static std::string currentScenePath;
	static void SaveScene(MainScene* scene, const std::string& path = "");
	static std::string SaveToMemory(MainScene* scene);
	static void LoadScene(MainScene* scene, const std::string& path);
	static void LoadFromMemory(MainScene* scene, const std::string& data);
	
	// ★追加: 実行ファイルの場所に関わらず必ずTD_Engineプロジェクトを指す絶対パスを取得
	static std::string GetUnifiedProjectPath(const std::string& path);
	static void AddScene(MainScene* scene, const std::string& path);
	static std::vector<entt::entity> LoadPrefab(MainScene* scene, const std::string& path);

	// ★追加: 初期化（アイコンのロードなど）
	static void Initialize(Engine::Renderer* renderer);

private:
	// アイコン用テクスチャハンドル
	struct Icons {
		uint32_t folder = 0;
		uint32_t file = 0;
		uint32_t model = 0;
		uint32_t prefab = 0;
		uint32_t audio = 0;
		uint32_t script = 0;
	};
	static Icons s_icons;

private:
	static void ShowHierarchy(MainScene* scene);
	static void ShowInspector(MainScene* scene);
	static void ShowProject(Engine::Renderer* renderer, MainScene* scene);
	static void ShowSceneSettings(Engine::Renderer* renderer);
	static void ShowAnimationWindow(Engine::Renderer* renderer, MainScene* scene);
	static void ShowConsole();
	static void ShowPlayModeMonitor(MainScene* scene); // ★追加
	static void DrawSelectionGizmo(Engine::Renderer* renderer, MainScene* scene);

	// ★追加: アセット選択用ヘルパー
	static std::vector<std::string> GetAssetsInDir(const std::string& root, const std::vector<std::string>& extensions);
	static bool AssetField(const char* label, std::string& path, const std::vector<std::string>& extensions);
};

} // namespace Game
