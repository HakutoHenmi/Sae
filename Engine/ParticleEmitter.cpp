#include "ParticleEmitter.h"
#include <fstream>
#include <cstdlib>

namespace Engine {

float ParticleEmitter::RandomFloat(float min, float max) {
	if (min >= max) return min;
	float r = (float)rand() / (float)RAND_MAX;
	return min + r * (max - min);
}

Vector3 ParticleEmitter::RandomVector3(const Vector3& base, const Vector3& variance) {
	return {
		base.x + RandomFloat(-variance.x, variance.x),
		base.y + RandomFloat(-variance.y, variance.y),
		base.z + RandomFloat(-variance.z, variance.z)
	};
}

void ParticleEmitter::Initialize(Renderer& renderer, const std::string& name) {
	renderer_ = &renderer;
	params.name = name;
	ApplySystemSettings();
}

void ParticleEmitter::ApplySystemSettings() {
	if (!renderer_) return;
	particleSystem_.Initialize(*renderer_, 1000, params.meshPath, params.texturePath, true, params.useBillboard);
	currentTexturePath_ = params.texturePath;
	currentBillboard_ = params.useBillboard;
}

void ParticleEmitter::Update(float dt) {
	// 設定が変わっていたら再初期化
	if (currentTexturePath_ != params.texturePath || currentBillboard_ != params.useBillboard) {
		ApplySystemSettings();
	}

	if (isPlaying && params.emitRate > 0.0f) {
		emitTimer_ += dt;
		float emitInterval = 1.0f / params.emitRate;
		while (emitTimer_ >= emitInterval) {
			emitTimer_ -= emitInterval;
			EmitBurst(1);
		}
	}

	particleSystem_.Update(dt);
}

void ParticleEmitter::Draw(const Camera& cam) {
	std::string shaderName = params.shaderName;
	if (shaderName.empty()) {
		shaderName = params.isAdditive ? "ParticleAdditive" : "Particle";
	}

	particleSystem_.Draw(cam, shaderName,
						 params.useUvAnim,
						 params.uvAnimCols,
						 params.uvAnimRows,
						 params.uvAnimFps);
}

void ParticleEmitter::EmitBurst(int count) {
	for(int i = 0; i < count; ++i) {
		Vector3 startVel = RandomVector3(params.startVelocity, params.velocityVariance);
		Vector3 startPos = params.position;

		// ★追加: 放出形状に基づく計算
		if (params.shape == EmissionShape::Sphere) {
			// 球体内のランダムな位置
			float theta = RandomFloat(0, 3.14159265f * 2.0f);
			float phi = std::acos(RandomFloat(-1.0f, 1.0f));
			float r = params.shapeRadius * std::pow(RandomFloat(0.0f, 1.0f), 1.0f/3.0f);
			Vector3 offset = {
				r * std::sin(phi) * std::cos(theta),
				r * std::sin(phi) * std::sin(theta),
				r * std::cos(phi)
			};
			startPos.x += offset.x; startPos.y += offset.y; startPos.z += offset.z;
			// 速度も放射状に少しブレさせる場合はここで加算可能
			startVel.x += offset.x * 0.5f; startVel.y += offset.y * 0.5f; startVel.z += offset.z * 0.5f;
		} 
		else if (params.shape == EmissionShape::Cone) {
			// 円錐状（簡易）
			float angle = RandomFloat(0, params.shapeAngle);
			float theta = RandomFloat(0, 3.14159265f * 2.0f);
			float r = params.shapeRadius * std::tan(angle);
			Vector3 dir = {
				r * std::cos(theta),
				1.0f, // 上方向を基本軸とする
				r * std::sin(theta)
			};
			// 正規化して初速方向に合わせる（ここでは簡易に上方向中心）
			float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
			dir.x /= len; dir.y /= len; dir.z /= len;

			// 初速ベクトルを円錐方向に回転させるなど
			float speed = std::sqrt(startVel.x*startVel.x + startVel.y*startVel.y + startVel.z*startVel.z);
			startVel = {dir.x * speed, dir.y * speed, dir.z * speed};

			// 位置は底面の円から
			float baseR = params.shapeRadius * std::sqrt(RandomFloat(0, 1));
			startPos.x += baseR * std::cos(theta);
			startPos.z += baseR * std::sin(theta);
		}

		Vector3 startScale = RandomVector3(params.startSize, params.startSizeVariance);
		Vector3 endScale = RandomVector3(params.endSize, params.endSizeVariance);
		float life = params.lifeTime + RandomFloat(-params.lifeTimeVariance, params.lifeTimeVariance);
		if (life <= 0.001f) life = 0.001f;

		Vector3 angVel = RandomVector3(params.angularVelocity, params.angularVelocityVariance);

		particleSystem_.Emit(
			startPos,
			startVel,
			params.acceleration,
			startScale, endScale,
			params.startColor, params.endColor,
			life, angVel, params.damping
		);
	}
}

bool ParticleEmitter::SaveToJson(const std::string& path) {
	nlohmann::json j;
	j["name"] = params.name;
	j["emitRate"] = params.emitRate;
	j["burstCount"] = params.burstCount;

	j["position"] = {params.position.x, params.position.y, params.position.z};

	j["shape"] = static_cast<int>(params.shape);
	j["shapeRadius"] = params.shapeRadius;
	j["shapeAngle"] = params.shapeAngle;

	j["lifeTime"] = params.lifeTime;
	j["lifeTimeVariance"] = params.lifeTimeVariance;

	j["startVelocity"] = {params.startVelocity.x, params.startVelocity.y, params.startVelocity.z};
	j["velocityVariance"] = {params.velocityVariance.x, params.velocityVariance.y, params.velocityVariance.z};
	
	j["acceleration"] = {params.acceleration.x, params.acceleration.y, params.acceleration.z};
	j["damping"] = params.damping;

	j["startSize"] = {params.startSize.x, params.startSize.y, params.startSize.z};
	j["startSizeVariance"] = {params.startSizeVariance.x, params.startSizeVariance.y, params.startSizeVariance.z};
	j["endSize"] = {params.endSize.x, params.endSize.y, params.endSize.z};
	j["endSizeVariance"] = {params.endSizeVariance.x, params.endSizeVariance.y, params.endSizeVariance.z};

	j["startColor"] = {params.startColor.x, params.startColor.y, params.startColor.z, params.startColor.w};
	j["endColor"] = {params.endColor.x, params.endColor.y, params.endColor.z, params.endColor.w};

	j["startRotation"] = {params.startRotation.x, params.startRotation.y, params.startRotation.z};
	j["angularVelocity"] = {params.angularVelocity.x, params.angularVelocity.y, params.angularVelocity.z};
	j["angularVelocityVariance"] = {params.angularVelocityVariance.x, params.angularVelocityVariance.y, params.angularVelocityVariance.z};

	j["meshPath"] = params.meshPath;
	j["texturePath"] = params.texturePath;
	j["shaderName"] = params.shaderName;
	j["useBillboard"] = params.useBillboard;
	j["isAdditive"] = params.isAdditive;

	j["useUvAnim"] = params.useUvAnim;
	j["uvAnimCols"] = params.uvAnimCols;
	j["uvAnimRows"] = params.uvAnimRows;
	j["uvAnimFps"] = params.uvAnimFps;

	std::ofstream file(path);
	if (!file.is_open()) return false;
	file << j.dump(4);
	return true;
}

bool ParticleEmitter::LoadFromJson(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) return false;

	nlohmann::json j;
	try {
		file >> j;

		auto getVec3 = [](const nlohmann::json& jArray) -> Vector3 {
			if (jArray.is_array() && jArray.size() >= 3) {
				return {jArray[0].get<float>(), jArray[1].get<float>(), jArray[2].get<float>()};
			}
			return {0,0,0};
		};
		auto getVec4 = [](const nlohmann::json& jArray) -> Vector4 {
			if (jArray.is_array() && jArray.size() >= 4) {
				return {jArray[0].get<float>(), jArray[1].get<float>(), jArray[2].get<float>(), jArray[3].get<float>()};
			}
			return {1,1,1,1};
		};

		if (j.contains("name")) params.name = j["name"].get<std::string>();
		if (j.contains("emitRate")) params.emitRate = j["emitRate"].get<float>();
		if (j.contains("burstCount")) params.burstCount = j["burstCount"].get<int>();

		if (j.contains("position")) params.position = getVec3(j["position"]);

		if (j.contains("shape")) params.shape = static_cast<EmissionShape>(j["shape"].get<int>());
		if (j.contains("shapeRadius")) params.shapeRadius = j["shapeRadius"].get<float>();
		if (j.contains("shapeAngle")) params.shapeAngle = j["shapeAngle"].get<float>();

		if (j.contains("lifeTime")) params.lifeTime = j["lifeTime"].get<float>();
		if (j.contains("lifeTimeVariance")) params.lifeTimeVariance = j["lifeTimeVariance"].get<float>();

		if (j.contains("startVelocity")) params.startVelocity = getVec3(j["startVelocity"]);
		if (j.contains("velocityVariance")) params.velocityVariance = getVec3(j["velocityVariance"]);
		
		if (j.contains("acceleration")) params.acceleration = getVec3(j["acceleration"]);
		if (j.contains("damping")) params.damping = j["damping"].get<float>();

		if (j.contains("startSize")) params.startSize = getVec3(j["startSize"]);
		if (j.contains("startSizeVariance")) params.startSizeVariance = getVec3(j["startSizeVariance"]);
		if (j.contains("endSize")) params.endSize = getVec3(j["endSize"]);
		if (j.contains("endSizeVariance")) params.endSizeVariance = getVec3(j["endSizeVariance"]);

		if (j.contains("startColor")) params.startColor = getVec4(j["startColor"]);
		if (j.contains("endColor")) params.endColor = getVec4(j["endColor"]);

		if (j.contains("startRotation")) params.startRotation = getVec3(j["startRotation"]);
		if (j.contains("angularVelocity")) params.angularVelocity = getVec3(j["angularVelocity"]);
		if (j.contains("angularVelocityVariance")) params.angularVelocityVariance = getVec3(j["angularVelocityVariance"]);

		if (j.contains("meshPath")) params.meshPath = j["meshPath"].get<std::string>();
		if (j.contains("texturePath")) params.texturePath = j["texturePath"].get<std::string>();
		if (j.contains("shaderName")) params.shaderName = j["shaderName"].get<std::string>();
		if (j.contains("useBillboard")) params.useBillboard = j["useBillboard"].get<bool>();
		if (j.contains("isAdditive")) params.isAdditive = j["isAdditive"].get<bool>();

		if (j.contains("useUvAnim")) params.useUvAnim = j["useUvAnim"].get<bool>();
		if (j.contains("uvAnimCols")) params.uvAnimCols = j["uvAnimCols"].get<int>();
		if (j.contains("uvAnimRows")) params.uvAnimRows = j["uvAnimRows"].get<int>();
		if (j.contains("uvAnimFps")) params.uvAnimFps = j["uvAnimFps"].get<float>();

		ApplySystemSettings();
		return true;

	} catch(...) {
		return false;
	}
}

} // namespace Engine

