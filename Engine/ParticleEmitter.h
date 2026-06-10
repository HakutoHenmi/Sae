#pragma once

#include "Particle.h"
#include <string>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26819)
#endif
#include "ThirdParty/nlohmann/json.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace Engine {

// ★追加: 放出形状の定義
enum class EmissionShape {
	Point = 0,
	Sphere = 1,
	Cone = 2
};

struct EmitterParams {
	std::string name = "Default Emitter";
	
	// Emission
	float emitRate = 10.0f; // particles per second
	int burstCount = 0;
	
	// Shape (★追加)
	EmissionShape shape = EmissionShape::Point;
	float shapeRadius = 1.0f;   // SphereやConeの底面半径
	float shapeAngle = 0.5f;    // Coneの広がり角 (ラジアン)
	
	// Transform
	Vector3 position{0, 0, 0};
	
	// Lifetime
	float lifeTime = 1.0f;
	float lifeTimeVariance = 0.2f;

	// Velocity
	Vector3 startVelocity{0, 1, 0};
	Vector3 velocityVariance{0.5f, 0.5f, 0.5f};
	
	// Acceleration & Physics
	Vector3 acceleration{0, -9.8f, 0};
	float damping = 0.0f; // ★追加: 速度の減衰(空気抵抗など)
	
	// Size
	Vector3 startSize{1, 1, 1};
	Vector3 startSizeVariance{0, 0, 0};
	Vector3 endSize{0, 0, 0};
	Vector3 endSizeVariance{0, 0, 0};
	
	// Color
	Vector4 startColor{1, 1, 1, 1};
	Vector4 endColor{1, 1, 1, 0};
	
	// Rotation
	Vector3 startRotation{0, 0, 0};
	Vector3 angularVelocity{0, 0, 0};
	Vector3 angularVelocityVariance{0, 0, 0};

	// Rendering
	std::string meshPath = "Resources/Models/plane.obj"; // ★追加: 放出するメッシュ
	std::string texturePath = "Resources/Textures/uvChecker.png";
	std::string shaderName = ""; // empty means default
	bool useBillboard = true;
	bool isAdditive = false; // Additive blending

	// ★追加: UVアニメーション
	bool useUvAnim = false;
	int uvAnimCols = 1;
	int uvAnimRows = 1;
	float uvAnimFps = 10.0f;
};

class ParticleEmitter {
public:
	void Initialize(Renderer& renderer, const std::string& name = "Emitter");
	void Update(float dt);
	void Draw(const Camera& cam);
	
	void EmitBurst(int count);
	
	bool SaveToJson(const std::string& path);
	bool LoadFromJson(const std::string& path);
	
	EmitterParams params;

	// 制御用
	bool isPlaying = true;
	
private:
	ParticleSystem particleSystem_;
	Renderer* renderer_ = nullptr;
	float emitTimer_ = 0.0f;
	std::string currentTexturePath_;
	bool currentBillboard_ = true;

	void ApplySystemSettings();
	float RandomFloat(float min, float max);
	Vector3 RandomVector3(const Vector3& base, const Vector3& variance);
};

} // namespace Engine

