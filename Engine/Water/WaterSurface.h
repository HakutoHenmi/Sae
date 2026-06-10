// Engine/Water/WaterSurface.h
#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace Engine {

class WindowDX; // 既存
class Camera;   // 既存

// 水面の基本設定
struct WaterSurfaceDesc {
	float sizeX = 400.0f;     // X方向の広さ
	float sizeZ = 400.0f;     // Z方向の広さ
	unsigned int tessX = 200; // 分割数（多いほど細かい）
	unsigned int tessZ = 200;
	float height = 0.0f; // 水面の Y 位置
};

class WaterSurface {
public:
	WaterSurface() = default;
	~WaterSurface() = default;

	bool Initialize(WindowDX& dx, const WaterSurfaceDesc& desc = WaterSurfaceDesc{});
	void Shutdown();

	// dt は 1/60.0f 固定でもOK
	void Update(float deltaSeconds);

	// Camera は既存のものを使う（Skybox / Voxel と同じ）
	void Draw(ID3D12GraphicsCommandList* cmd, const Camera& cam);

private:
	struct Vertex {
		DirectX::XMFLOAT3 pos; // x,z 平面の頂点
		DirectX::XMFLOAT2 uv;
	};

	// b0: WVP + 色
	struct CBCommon {
		DirectX::XMFLOAT4X4 mvp;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT4 camPos; // xyz: カメラ位置
	};

	// b1: 波パラメータ + 時間
	//
	// wave1 : dirX, dirZ, amplitude, frequency
	// wave2 : dirX, dirZ, amplitude, frequency
	// misc  : time, waterHeight, reserved, reserved
	struct CBWave {
		DirectX::XMFLOAT4 wave1;
		DirectX::XMFLOAT4 wave2;
		DirectX::XMFLOAT4 misc;
	};

	bool createMesh_(WindowDX& dx);
	bool createPipeline_(WindowDX& dx);

private:
	WindowDX* dx_ = nullptr;
	WaterSurfaceDesc desc_{};

	// ジオメトリ
	Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
	D3D12_VERTEX_BUFFER_VIEW vbv_{};
	D3D12_INDEX_BUFFER_VIEW ibv_{};
	unsigned int indexCount_ = 0;

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> cbCommon_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cbWave_;

	// ルートシグネチャ / PSO
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rs_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

	// 内部状態
	float time_ = 0.0f;
	CBWave waveParam_{};
};

} // namespace Engine
