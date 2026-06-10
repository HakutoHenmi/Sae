#pragma once
#include <string>
#include <vector>

#include "Camera.h"
#include "Matrix4x4.h"
#include "Renderer.h"
#include "Transform.h"
#include "WindowDX.h"

namespace Engine {

struct Particle {
	Vector3 pos{};
	Vector3 vel{};
	Vector3 acceleration{};

	Vector3 startScale{1, 1, 1};
	Vector3 endScale{0, 0, 0};
	Vector3 scale{1, 1, 1};

	Vector4 startColor{1, 1, 1, 1};
	Vector4 endColor{1, 1, 1, 0};
	Vector4 color{1, 1, 1, 1};

	float life = 1.0f;
	float age = 0.0f;
	bool active = false;

	float damping = 0.0f; // ★追加: 摩擦・空気抵抗

	// ★追加: 回転制御用
	Vector3 rotation{}; // 現在の回転角 (ラジアン)
	Vector3 angVel{};   // 回転速度
};

class ParticleSystem {
public:
	// texturePath は任意。存在する png を指定してください（例: "Resources/Textures/uvChecker.png"）
	// ★変更: useBillboard 引数を追加（デフォルトtrue）
	void Initialize(
	    Renderer& renderer, size_t maxCount = 1000, const std::string& meshPath = "Resources/Models/plane.obj", const std::string& texturePath = "Resources/Textures/uvChecker.png", bool sRGB = true,
	    bool useBillboard = true);

	void Update(float dt);
	void Draw(const Camera& cam);
	void Draw(const Camera& cam, const std::string& shaderName, bool useUvAnim = false, 
			  int uvCols = 1, int uvRows = 1, float uvFps = 10.0f);
	

	void Clear();

	// ★変更: 回転速度(angVel)引数と摩擦(damping)引数を追加
	void Emit(const Vector3& pos, const Vector3& vel, const Vector3& acceleration, 
			  const Vector3& startScale, const Vector3& endScale,
			  const Vector4& startColor, const Vector4& endColor, 
			  float life, const Vector3& angVel = {0, 0, 0}, float damping = 0.0f);

private:
	Renderer* renderer_ = nullptr;

	Renderer::MeshHandle mesh_ = 0;
	Renderer::TextureHandle tex_ = 0;

	std::vector<Particle> particles_;

	// ★追加: ビルボードを使用するかどうか
	bool useBillboard_ = true;
};

} // namespace Engine
