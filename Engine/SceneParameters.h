#pragma once
#include <string>

namespace Engine {

// シーン遷移時に引き渡すパラメータ群
struct SceneParameters {
	std::string stagePath;    // 読み込むステージのパス
	bool isWin = false;       // ゲームの勝敗結果
	float clearTime = 0.0f;   // クリアした時間
	int score = 0;            // 獲得スコア
	
	// 必要に応じて拡張可能
};

} // namespace Engine
