#pragma once
// ================================================
// IScene.h
//   シーン共通インターフェイス
//   ・Initialize / Update / Draw の3本柱
//   ・終了/次シーン名の取り決め
// ================================================
#include <string>
#include "WindowDX.h"
#include "SceneParameters.h"

namespace Engine {

// シーンの基底クラス（ゲーム側で継承して実装）
class IScene {
public:
	virtual ~IScene() = default;

	// 初期化（リソース確保・初期状態のセット）
	virtual void Initialize(WindowDX* dx, const SceneParameters& params) = 0;

	// 毎フレーム更新（入力処理・状態遷移・ゲームロジック）
	virtual void Update() = 0;

	// 毎フレーム描画（3D/2D/UI をここで）
	virtual void Draw() = 0;

	// エディターUI描画（デバッグ用）
	virtual void DrawEditor() {}
	
	// ワールド空間UI描画（リリース共用）
	virtual void DrawUI() {}

	// このシーンを終了するか？（true で次のシーンへ）
	// ※ ゲーム側が任意の条件で true を返す
	virtual bool IsEnd() const { return false; }

	// 次に遷移するシーン名（IsEnd()==true の時に参照）
	// 例: "Title", "Game", "Result" など
	virtual std::string Next() const { return std::string(); }
};

} // namespace Engine
