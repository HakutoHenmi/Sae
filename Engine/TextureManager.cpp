#include "TextureManager.h"
#include <cassert>
#include <d3dx12.h>
#include <filesystem>

#pragma comment(lib, "DirectXTex.lib")

namespace Engine {

using Microsoft::WRL::ComPtr;

static std::wstring AssetFullPath(const std::wstring& rel) {
	wchar_t buf[32768]{};
	DWORD len = ::GetModuleFileNameW(nullptr, buf, 32768);
	if (len == 0 || len >= 32768) {
		return (std::filesystem::current_path() / rel).wstring();
	}
	buf[len] = L'\0';
	std::filesystem::path exeDir = std::filesystem::path(buf).parent_path();
	return (exeDir / rel).wstring();
}

TextureManager& TextureManager::Instance() {
	static TextureManager inst;
	return inst;
}

void TextureManager::Initialize(WindowDX* dx, Renderer* renderer) {
	dx_ = dx;
	renderer_ = renderer; // 互換のため保持
	pathToIndex_.clear();
	textures_.clear();
	srvCursor_ = 10;
}

void TextureManager::Shutdown() {
	// GPU参照中に解放しない
	if (dx_)
		dx_->WaitIdle();

	textures_.clear();
	pathToIndex_.clear();
	dx_ = nullptr;
	renderer_ = nullptr;
	srvCursor_ = 10;
}

TextureHandle TextureManager::Load(const std::wstring& relPath) {
	TextureHandle handle;

	if (!dx_)
		return handle;

	// 既に読み込み済みならそれを返す
	auto it = pathToIndex_.find(relPath);
	if (it != pathToIndex_.end()) {
		handle.index = it->second;
		return handle;
	}

	// 実ファイルパス
	std::wstring full = AssetFullPath(relPath);

	// 画像読み込み（互換のため FORCE_SRGB を維持）
	DirectX::ScratchImage img;
	HRESULT hr = DirectX::LoadFromWICFile(full.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, img);
	if (FAILED(hr)) {
		return handle; // invalid
	}

	const auto& meta = img.GetMetadata();

	ID3D12Device* dev = dx_->Dev();
	if (!dev)
		return handle;

	// テクスチャリソース作成
	CD3DX12_HEAP_PROPERTIES hpD(D3D12_HEAP_TYPE_DEFAULT);
	auto rdTex = CD3DX12_RESOURCE_DESC::Tex2D(meta.format, meta.width, (UINT)meta.height);

	ComPtr<ID3D12Resource> tex;
	hr = dev->CreateCommittedResource(&hpD, D3D12_HEAP_FLAG_NONE, &rdTex, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
	if (FAILED(hr))
		return handle;

	// アップロード用バッファ
	UINT64 upSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);
	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);
	auto rdUp = CD3DX12_RESOURCE_DESC::Buffer(upSize);

	ComPtr<ID3D12Resource> up;
	hr = dev->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rdUp, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&up));
	if (FAILED(hr))
		return handle;

	// サブリソース設定（mip無しの1枚）
	const auto* image0 = img.GetImage(0, 0, 0);
	if (!image0)
		return handle;

	D3D12_SUBRESOURCE_DATA sd{};
	sd.pData = image0->pixels;
	sd.RowPitch = (LONG_PTR)image0->rowPitch;
	sd.SlicePitch = (LONG_PTR)image0->slicePitch;

	// コマンドリストへコピーを積む（呼び出し側が BeginFrame〜EndFrame の中で呼ぶ前提）
	ID3D12GraphicsCommandList* cmd = dx_->List();
	if (!cmd)
		return handle;

	UpdateSubresources(cmd, tex.Get(), up.Get(), 0, 0, 1, &sd);

	auto bar = CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmd->ResourceBarrier(1, &bar);

	// SRVスロット確保（WindowDX の SRV ヒープを直使用）
	const int srvIndex = srvCursor_++;

	D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
	sv.Format = meta.format;
	sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sv.Texture2D.MipLevels = 1;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu = dx_->SRV_CPU(srvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpu = dx_->SRV_GPU(srvIndex);
	dev->CreateShaderResourceView(tex.Get(), &sv, cpu);

	// 登録
	TexData td;
	td.texture = tex;
	td.upload = up; // ★保持して device removed を避ける
	td.srvIndex = srvIndex;
	td.gpu = gpu;

	const int newIndex = (int)textures_.size();
	textures_.push_back(td);
	pathToIndex_[relPath] = newIndex;

	handle.index = newIndex;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetGPU(const TextureHandle& h) const {
	D3D12_GPU_DESCRIPTOR_HANDLE dummy{};
	if (!IsValid(h))
		return dummy;
	return textures_[h.index].gpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetCPU(const TextureHandle& h) const {
	D3D12_CPU_DESCRIPTOR_HANDLE dummy{};
	if (!IsValid(h))
		return dummy;
	if (!dx_)
		return dummy;
	return dx_->SRV_CPU(textures_[h.index].srvIndex);
}

} // namespace Engine
