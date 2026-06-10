#pragma once
#include <DirectXTex.h>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

#include "Renderer.h"
#include "WindowDX.h"

namespace Engine {

// テクスチャ1枚ぶんの情報
struct TextureHandle {
	int index = -1; // 内部ID（配列インデックス）
};

class TextureManager {
public:
	static TextureManager& Instance();

	void Initialize(WindowDX* dx, Renderer* renderer);
	void Shutdown();

	// relPath: exeと同じフォルダからの相対パス(L"Resources/sample.png" など)
	TextureHandle Load(const std::wstring& relPath);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPU(const TextureHandle& h) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPU(const TextureHandle& h) const;

	Renderer* GetRenderer() const { return renderer_; }
	bool IsValid(const TextureHandle& h) const { return (h.index >= 0 && h.index < (int)textures_.size()); }

private:
	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	struct TexData {
		Microsoft::WRL::ComPtr<ID3D12Resource> texture;

		// ★安全最優先：upload を保持して device removed を避ける（WindowDX が毎フレーム WaitGPU するので本来は後で解放可能）
		// ただし TextureManager はフレーム境界を知らないので、ここでは保持し続ける。
		// 将来的に最適化するなら「フレーム終端でGC」関数を追加して解放する。
		Microsoft::WRL::ComPtr<ID3D12Resource> upload;

		int srvIndex = -1; // WindowDX SRVヒープのオフセット
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
	};

	WindowDX* dx_ = nullptr;
	Renderer* renderer_ = nullptr;

	// SRV は WindowDX のヒープを使用。0..3 は予約済みなので、ゲーム用は 10 から。
	int srvCursor_ = 10;

	std::unordered_map<std::wstring, int> pathToIndex_;
	std::vector<TexData> textures_;
};

} // namespace Engine
