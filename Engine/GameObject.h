// Game/GameObject.h
#pragma once
#include "Transform.h"
#include <cstdint>
#include <string>

// 前方宣言
namespace Game {
class GimmickBase;
}

namespace Engine {

struct GameObject {
	std::string name = "Object";
	Transform transform;

	uint32_t meshHandle = 0;
	uint32_t textureHandle = 0;

	uint32_t type = 0;
	bool isVisible = true;

	Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
	std::string textureName = "";

	// 使用するシェーダー名 (デフォルトは "Default")
	std::string shaderName = "Default";

	// AABB（簡易判定用）
	Vector3 localAABBMin = {-1.0f, -1.0f, -1.0f};
	Vector3 localAABBMax = {1.0f, 1.0f, 1.0f};

	// 詳細なメッシュコライダーデータへのポインタ
	const void* collisionMesh = nullptr;

	// ギミックシステム用
	// ロジック本体へのポインタ (実行時用)
	Game::GimmickBase* gimmick = nullptr;
	// ロジックのクラス名 (保存・ロード・エディタ表示用)
	std::string gimmickName = "";

	// エディター用ロックフラグ
	bool isLocked = false;

	// メッシュ衝突判定を使用するかどうかのフラグ
	bool useMeshCollision = true;

	// ★追加: 3Dモデルのファイル名 (リネームしてもモデルを見失わないようにするため)
	std::string modelFileName = "";
};

} // namespace Engine
