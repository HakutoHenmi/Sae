// Engine/DynamicGlyphCache.cpp
#include "DynamicGlyphCache.h"
#include "Renderer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include <d3dx12.h>

namespace Engine {

bool DynamicGlyphCache::Initialize(Renderer* renderer, const std::string& fontPath, float pixelHeight,
                                   uint32_t atlasWidth, uint32_t atlasHeight) {
	renderer_ = renderer;
	pixelHeight_ = pixelHeight;
	atlasWidth_ = atlasWidth;
	atlasHeight_ = atlasHeight;

	if (!font_.Load(fontPath)) {
		return false;
	}

	font_.GetVerticalMetrics(pixelHeight_, ascent_, descent_, lineGap_);

	// CPU側アトラスバッファ (R8 グレースケール)
	cpuAtlas_.resize(static_cast<size_t>(atlasWidth_) * atlasHeight_, 0);

	// GPU テクスチャの作成
	auto* dev = renderer_->GetDevice();
	if (!dev) return false;

	D3D12_RESOURCE_DESC texDesc{};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = atlasWidth_;
	texDesc.Height = atlasHeight_;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8_UNORM; // グレースケール
	texDesc.SampleDesc.Count = 1;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
	HRESULT hr = dev->CreateCommittedResource(
		&heapProp, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
		IID_PPV_ARGS(&atlasTexture_));
	if (FAILED(hr)) return false;

	// SRV の作成 (Renderer の AllocateSrvIndex に相当する処理)
	// Renderer は WindowDX 経由でヒープ管理しているのでその仕組みに乗る
	// ここでは Renderer::LoadTexture2D と同様のパターンを使う
	// → Renderer に友好メソッドを追加するか、直接ハンドルを取得する

	// Renderer からSRVインデックスを割り当てる
	// Renderer の public メソッドを通じてアクセス
	auto srvIdx = renderer_->AllocateTextSrvIndex();

	auto* window = renderer_->GetWindow();
	if (!window) return false;

	atlasSrvCpu_ = window->SRV_CPU(static_cast<int>(srvIdx));
	atlasSrvCpuMaster_ = window->SRV_CPU_Master(static_cast<int>(srvIdx));
	atlasSrvGpu_ = window->SRV_GPU(static_cast<int>(srvIdx));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	dev->CreateShaderResourceView(atlasTexture_.Get(), &srvDesc, atlasSrvCpu_);
	dev->CreateShaderResourceView(atlasTexture_.Get(), &srvDesc, atlasSrvCpuMaster_);

	cursorX_ = 1; // 左端に1px余白
	cursorY_ = 1; // 上端に1px余白
	rowHeight_ = 0;

	initialized_ = true;
	return true;
}

const CachedGlyph* DynamicGlyphCache::GetGlyph(uint32_t codepoint) {
	if (!initialized_) return nullptr;

	// キャッシュにある場合はそのまま返す
	auto it = cache_.find(codepoint);
	if (it != cache_.end()) {
		return &it->second;
	}

	// キャッシュにない → ラスタライズしてアトラスに追加
	CachedGlyph glyph;
	if (!AddGlyphToAtlas(codepoint, glyph)) {
		return nullptr;
	}

	auto [insertIt, success] = cache_.emplace(codepoint, glyph);
	return &insertIt->second;
}

bool DynamicGlyphCache::AddGlyphToAtlas(uint32_t codepoint, CachedGlyph& outGlyph) {
	GlyphMetrics metrics;
	auto bitmap = font_.RasterizeGlyph(codepoint, pixelHeight_, metrics);

	outGlyph.metrics = metrics;

	// ビットマップが空の場合（スペースなど）
	if (bitmap.empty() || metrics.width == 0 || metrics.height == 0) {
		outGlyph.hasBitmap = false;
		outGlyph.u0 = outGlyph.v0 = outGlyph.u1 = outGlyph.v1 = 0.0f;
		return true;
	}

	uint32_t gw = static_cast<uint32_t>(metrics.width);
	uint32_t gh = static_cast<uint32_t>(metrics.height);
	uint32_t padding = 1; // グリフ間のパディング

	// 現在の行に収まるか確認
	if (cursorX_ + gw + padding > atlasWidth_) {
		// 次の行へ
		cursorX_ = 1;
		cursorY_ += rowHeight_ + padding;
		rowHeight_ = 0;
	}

	// テクスチャの高さを超えたら失敗
	if (cursorY_ + gh + padding > atlasHeight_) {
		return false; // アトラスが満杯
	}

	// CPU アトラスにグリフデータを書き込む
	for (uint32_t y = 0; y < gh; ++y) {
		for (uint32_t x = 0; x < gw; ++x) {
			uint32_t atlasIdx = (cursorY_ + y) * atlasWidth_ + (cursorX_ + x);
			cpuAtlas_[atlasIdx] = bitmap[y * gw + x];
		}
	}

	// GPU テクスチャに部分アップロード
	UploadAtlasRegion(cursorX_, cursorY_, gw, gh, bitmap.data());

	// UV座標を計算
	float invW = 1.0f / static_cast<float>(atlasWidth_);
	float invH = 1.0f / static_cast<float>(atlasHeight_);
	outGlyph.u0 = static_cast<float>(cursorX_) * invW;
	outGlyph.v0 = static_cast<float>(cursorY_) * invH;
	outGlyph.u1 = static_cast<float>(cursorX_ + gw) * invW;
	outGlyph.v1 = static_cast<float>(cursorY_ + gh) * invH;
	outGlyph.hasBitmap = true;

	// カーソルを進める
	cursorX_ += gw + padding;
	rowHeight_ = (std::max)(rowHeight_, gh);

	return true;
}

void DynamicGlyphCache::UploadAtlasRegion(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t* data) {
	auto* dev = renderer_->GetDevice();
	auto* queue = renderer_->GetQueue();
	if (!dev || !queue) return;

	// アップロードバッファの作成
	// 行ピッチは D3D12 の要件に合わせて 256 バイトアラインメント
	UINT rowPitch = (w + 255) & ~255u; // 256の倍数に切り上げ
	UINT64 uploadSize = static_cast<UINT64>(rowPitch) * h;

	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuf;
	CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
	dev->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &bufDesc,
	                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                             IID_PPV_ARGS(&uploadBuf));

	// アップロードバッファにデータをコピー
	uint8_t* mapped = nullptr;
	uploadBuf->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	for (uint32_t row = 0; row < h; ++row) {
		std::memcpy(mapped + row * rowPitch, data + row * w, w);
	}
	uploadBuf->Unmap(0, nullptr);

	// コマンドリストを作成して転送
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmd;
	dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
	dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd));

	// バリア: SRV → COPY_DEST
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		atlasTexture_.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST);
	cmd->ResourceBarrier(1, &barrier);

	// テクスチャの一部にコピー
	D3D12_TEXTURE_COPY_LOCATION dst{};
	dst.pResource = atlasTexture_.Get();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION src{};
	src.pResource = uploadBuf.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset = 0;
	src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
	src.PlacedFootprint.Footprint.Width = w;
	src.PlacedFootprint.Footprint.Height = h;
	src.PlacedFootprint.Footprint.Depth = 1;
	src.PlacedFootprint.Footprint.RowPitch = rowPitch;

	D3D12_BOX srcBox = { 0, 0, 0, w, h, 1 };
	cmd->CopyTextureRegion(&dst, x, y, 0, &src, &srcBox);

	// バリア: COPY_DEST → SRV
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		atlasTexture_.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmd->ResourceBarrier(1, &barrier);

	cmd->Close();
	ID3D12CommandList* ppLists[] = { cmd.Get() };
	queue->ExecuteCommandLists(1, ppLists);

	// GPU完了を待つ
	// 注意: 毎文字でGPU同期するのはパフォーマンスに良くないが、
	//       初回ロード時のみの処理なので許容範囲
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, ev);
		WaitForSingleObject(ev, INFINITE);
	}
	CloseHandle(ev);
}

} // namespace Engine
