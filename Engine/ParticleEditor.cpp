#include "ParticleEditor.h"
#include "../externals/imgui/imgui.h"
#include <algorithm>

namespace Engine {

void ParticleEditor::Initialize() {
	previewEmitter_.Initialize(*Renderer::GetInstance(), "PreviewEmitter");
	targetEmitter = &previewEmitter_;

	// ★追加: プレビュー用レンダーターゲットとカメラの初期化
	previewTarget_ = Renderer::GetInstance()->CreateRenderTarget(512, 512);
	// GPUParticleの初期化
	gpuParticleSystem_.Initialize(Renderer::GetInstance()->GetDevice(), 10000);

	previewCamera_.Initialize();
	// ★修正: 透視投影行列を設定（これがないと何も描画されない）
	previewCamera_.SetProjection(0.7854f, 1.0f, 0.1f, 200.0f); // FOV=45度, aspect=1:1
	previewCamera_.SetPosition(0, 2, -10);
}

void ParticleEditor::Update(float dt) {
	if (targetEmitter == &previewEmitter_) {
		GPUParticleEmitterData ed{};
		ed.emitPos = { targetEmitter->params.position.x, targetEmitter->params.position.y, targetEmitter->params.position.z };
		ed.emitRate = targetEmitter->params.emitRate;
		ed.emitVel = { targetEmitter->params.startVelocity.x, targetEmitter->params.startVelocity.y, targetEmitter->params.startVelocity.z };
		ed.emitLife = targetEmitter->params.lifeTime;
		gpuParticleSystem_.Update(Renderer::GetInstance()->GetCommandList(), dt, ed);
	}

	// ★変更: ImGui コンテキストがない場合は以降の入力をスキップ
	if (!ImGui::GetCurrentContext()) {
		previewCamera_.SetPosition(0, 2, -10);
		previewCamera_.LookAt(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		return;
	}

	// ★追加: カメラ操作 (ImGuiウィンドウ上でドラッグ可能にする)
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
		ImVec2 delta = ImGui::GetIO().MouseDelta;
		camRotX_ -= delta.y * 0.01f;
		camRotY_ -= delta.x * 0.01f;
	}
	camDist_ -= ImGui::GetIO().MouseWheel * 1.0f;
	camDist_ = (std::max)(1.0f, camDist_);

	// カメラ座標の計算 (極座標から直交座標)
	Vector3 pos;
	pos.x = camDist_ * cosf(camRotX_) * sinf(camRotY_);
	pos.y = camDist_ * sinf(camRotX_);
	pos.z = camDist_ * cosf(camRotX_) * cosf(camRotY_);
	
	previewCamera_.SetPosition(pos.x, pos.y, pos.z);
	// ★修正: LookAtで常に原点方向を向かせる（SetRotationだとズレやすい）
	previewCamera_.LookAt(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
}

void ParticleEditor::DrawPreview(const Camera& cam) {
	(void)cam; // unused
	// ★変更: このDrawPreview自体はメイン描画からは呼ばれないが、
	// Renderer内部の独自パスでオフスクリーン描画を実行する
}

void ParticleEditor::DrawUI() {
	if (!targetEmitter) return;

	// Removed ImGui::Begin("Particle Editor") to support tab embedding

	// ★追加: プレビュー映像の描画
	ImGui::Text("Preview (Right-click drag to rotate, Scroll to zoom)");
	ImVec2 previewSize(512, 512);
	
	// レンダリング先をこのウィンドウのテクスチャに変更
	Renderer::GetInstance()->BeginCustomRenderTarget(previewTarget_);

	// ★追加: プレビュー用カメラを適用
	Renderer::GetInstance()->SetCamera(previewCamera_);

	// ★追加: グリッド線の描画（XZ平面、-5〜+5）
	auto* r = Renderer::GetInstance();
	const float gridSize = 5.0f;
	const float gridStep = 1.0f;
	Vector4 gridColor = {0.35f, 0.35f, 0.35f, 1.0f};
	for (float i = -gridSize; i <= gridSize; i += gridStep) {
		r->DrawLine3D({i, 0, -gridSize}, {i, 0, gridSize}, gridColor);
		r->DrawLine3D({-gridSize, 0, i}, {gridSize, 0, i}, gridColor);
	}

	// ★追加: 原点ギズモ（XYZ軸の表示）
	const float axisLen = 2.0f;
	r->DrawLine3D({0,0,0}, {axisLen,0,0}, {1.0f, 0.2f, 0.2f, 1.0f}); // X: 赤
	r->DrawLine3D({0,0,0}, {0,axisLen,0}, {0.2f, 1.0f, 0.2f, 1.0f}); // Y: 緑
	r->DrawLine3D({0,0,0}, {0,0,axisLen}, {0.3f, 0.3f, 1.0f, 1.0f}); // Z: 青

	// ★修正: 先に FlushDrawCalls() を呼び、Skyboxやグリッドなどを描画させてから、
	// その上にパーティクルを描画するようにする（Skyboxにパーティクルが上書きされるのを防ぐため）
	r->FlushDrawCalls();

	// パーティクルの描画 (常にGPU)
	if (targetEmitter == &previewEmitter_) {
		DirectX::XMMATRIX vp = previewCamera_.View() * previewCamera_.Proj();
		Matrix4x4 viewProjMat;
		DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&viewProjMat), vp);

		Vector3 pos = previewCamera_.GetPosition();
		DirectX::XMFLOAT3 camPos(pos.x, pos.y, pos.z);

		gpuParticleSystem_.Draw(Renderer::GetInstance()->GetCommandList(), viewProjMat, camPos, targetEmitter->params.useBillboard);
	}

	Renderer::GetInstance()->EndCustomRenderTarget();

	// ImGui上に画像として表示
	ImGui::Image((ImTextureID)previewTarget_.srvGpu.ptr, previewSize);

	if (ImGui::CollapsingHeader("File", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::InputText("File Path", filePathBuf_, sizeof(filePathBuf_));
		if (ImGui::Button("Save JSON")) {
			targetEmitter->SaveToJson(filePathBuf_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Load JSON")) {
			targetEmitter->LoadFromJson(filePathBuf_);
		}
	}

	if (ImGui::CollapsingHeader("Playback", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("GPU Particle Active");
		ImGui::Checkbox("Is Playing", &targetEmitter->isPlaying);
		if (ImGui::Button("Emit Burst (10)")) {
			targetEmitter->EmitBurst(10);
		}
	}

	if (ImGui::CollapsingHeader("Emission & Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("Emit Rate (Hz)", &targetEmitter->params.emitRate, 0.1f, 0.0f, 1000.0f);
		ImGui::DragFloat("Life Time", &targetEmitter->params.lifeTime, 0.01f, 0.1f, 10.0f);
		ImGui::DragFloat("Life Time Variance", &targetEmitter->params.lifeTimeVariance, 0.01f, 0.0f, 5.0f);

		// ★追加: 形状
		int shapeType = static_cast<int>(targetEmitter->params.shape);
		const char* shapeNames[] = { "Point", "Sphere", "Cone" };
		if (ImGui::Combo("Emission Shape", &shapeType, shapeNames, IM_ARRAYSIZE(shapeNames))) {
			targetEmitter->params.shape = static_cast<EmissionShape>(shapeType);
		}
		if (targetEmitter->params.shape != EmissionShape::Point) {
			ImGui::DragFloat("Shape Radius", &targetEmitter->params.shapeRadius, 0.01f, 0.0f, 100.0f);
		}
		if (targetEmitter->params.shape == EmissionShape::Cone) {
			ImGui::DragFloat("Cone Angle (Rad)", &targetEmitter->params.shapeAngle, 0.01f, 0.0f, 3.1415f);
		}
	}

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Position", &targetEmitter->params.position.x, 0.1f);
	}

	if (ImGui::CollapsingHeader("Velocity & Acceleration", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Start Velocity", &targetEmitter->params.startVelocity.x, 0.1f);
		ImGui::DragFloat3("Velocity Variance", &targetEmitter->params.velocityVariance.x, 0.1f);
		ImGui::DragFloat3("Acceleration", &targetEmitter->params.acceleration.x, 0.1f);
		ImGui::DragFloat("Damping", &targetEmitter->params.damping, 0.01f, 0.0f, 100.0f); // ★追加: 摩擦
	}

	if (ImGui::CollapsingHeader("Size", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Start Size", &targetEmitter->params.startSize.x, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("Start Size Variance", &targetEmitter->params.startSizeVariance.x, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("End Size", &targetEmitter->params.endSize.x, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("End Size Variance", &targetEmitter->params.endSizeVariance.x, 0.01f, 0.0f, 10.0f);
	}

	if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("Start Color", &targetEmitter->params.startColor.x);
		ImGui::ColorEdit4("End Color", &targetEmitter->params.endColor.x);
	}

	if (ImGui::CollapsingHeader("Rotation", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Angular Velocity", &targetEmitter->params.angularVelocity.x, 0.01f);
		ImGui::DragFloat3("Angular Vel Variance", &targetEmitter->params.angularVelocityVariance.x, 0.01f);
	}

	if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
		char texBuf[256];
		strcpy_s(texBuf, targetEmitter->params.texturePath.c_str());
		if (ImGui::InputText("Texture Path", texBuf, sizeof(texBuf))) {
			targetEmitter->params.texturePath = texBuf;
		}

		// ★変更: シェーダー選択ドロップダウン
		struct ShaderOption {
			const char* label;
			const char* shaderName;
			bool isAdditive;
		};
		static const ShaderOption shaderOptions[] = {
			{ "(Default) Particle",              "",                        false },
			{ "Particle (Alpha Blend)",          "Particle",               false },
			{ "Particle (Additive)",             "ParticleAdditive",       true  },
			{ "Procedural Smoke",                "ProceduralSmoke",        false },
			{ "Procedural Smoke (Additive)",     "ProceduralSmokeAdditive",true  },
		};
		static const int shaderOptionCount = IM_ARRAYSIZE(shaderOptions);

		// 現在のシェーダー名からインデックスを検索
		int currentShaderIdx = 0;
		for (int i = 0; i < shaderOptionCount; ++i) {
			if (targetEmitter->params.shaderName == shaderOptions[i].shaderName) {
				currentShaderIdx = i;
				break;
			}
		}

		if (ImGui::Combo("Shader Type", &currentShaderIdx, [](void* data, int idx, const char** out) -> bool {
			const ShaderOption* opts = (const ShaderOption*)data;
			*out = opts[idx].label;
			return true;
		}, (void*)shaderOptions, shaderOptionCount)) {
			targetEmitter->params.shaderName = shaderOptions[currentShaderIdx].shaderName;
			targetEmitter->params.isAdditive = shaderOptions[currentShaderIdx].isAdditive;

			// ★追加: ProceduralSmoke系を選択した場合、煙用プリセットを適用
			std::string sn = shaderOptions[currentShaderIdx].shaderName;
			if (sn == "ProceduralSmoke" || sn == "ProceduralSmokeAdditive") {
				targetEmitter->params.startVelocity = {0, 1.5f, 0};
				targetEmitter->params.velocityVariance = {0.3f, 0.3f, 0.3f};
				targetEmitter->params.acceleration = {0, 0.5f, 0}; // 上昇気流
				targetEmitter->params.damping = 1.0f;
				targetEmitter->params.lifeTime = 3.0f;
				targetEmitter->params.lifeTimeVariance = 0.5f;
				targetEmitter->params.startSize = {1.5f, 1.5f, 1.5f};
				targetEmitter->params.endSize = {4.0f, 4.0f, 4.0f};
				targetEmitter->params.startColor = {0.85f, 0.9f, 0.95f, 0.7f};
				targetEmitter->params.endColor = {0.6f, 0.65f, 0.7f, 0.0f};
				targetEmitter->params.emitRate = 8.0f;
			}
		}

		ImGui::Checkbox("Use Billboard", &targetEmitter->params.useBillboard);

		// ★追加: UVアニメーション
		ImGui::Separator();
		ImGui::Checkbox("Use UV Animation", &targetEmitter->params.useUvAnim);
		if (targetEmitter->params.useUvAnim) {
			ImGui::DragInt("Columns", &targetEmitter->params.uvAnimCols, 0.1f, 1, 64);
			ImGui::DragInt("Rows", &targetEmitter->params.uvAnimRows, 0.1f, 1, 64);
			ImGui::DragFloat("Animation FPS", &targetEmitter->params.uvAnimFps, 0.1f, 0.1f, 120.0f);
		}
	}

	// Removed ImGui::End() to support tab embedding
}

} // namespace Engine
