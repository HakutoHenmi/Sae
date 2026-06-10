#pragma once
#include "ParticleEmitter.h"
#include "GPUParticle.h"
#include <string>

namespace Engine {

class ParticleEditor {
public:
	void Initialize();
	void Update(float dt);
	void DrawUI();
	void DrawPreview(const Camera& cam);

	// ★追加: 編集用・プレビュー用のエミッター（対象が外部から与えられない場合に使用）
	ParticleEmitter previewEmitter_;
	GPUParticleSystem gpuParticleSystem_;

	// 編集対象のエミッター（コンポーネント用、nullptrでもよい）
	ParticleEmitter* targetEmitter = nullptr;

private:
	char filePathBuf_[256] = "Resources/Scenes/particle.json";

	// ★追加: プレビュー用のレンダーターゲットとカメラ
	Renderer::CustomRenderTarget previewTarget_;
	Camera previewCamera_;
	float camRotX_ = 0.5f;
	float camRotY_ = -0.5f;
	float camDist_ = 10.0f;
};

} // namespace Engine

