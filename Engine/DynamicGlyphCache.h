// Engine/DynamicGlyphCache.h
// 動的グリフキャッシュ - GPU テクスチャアトラスの管理
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

#include "Font.h"

namespace Engine {

class Renderer;

// アトラス上のグリフ情報
struct CachedGlyph {
	float u0 = 0.0f, v0 = 0.0f; // テクスチャ座標 (左上)
	float u1 = 0.0f, v1 = 0.0f; // テクスチャ座標 (右下)

	GlyphMetrics metrics{};      // グリフの配置情報 (ピクセル単位)
	bool hasBitmap = false;      // ビットマップを持っているか (スペースなどは false)
};

class DynamicGlyphCache {
public:
	DynamicGlyphCache() = default;
	~DynamicGlyphCache() = default;

	DynamicGlyphCache(const DynamicGlyphCache&) = delete;
	DynamicGlyphCache& operator=(const DynamicGlyphCache&) = delete;

	// 初期化
	// renderer: D3D12デバイスやSRV割り当てのために使用
	// fontPath: 使用するフォントファイルのパス
	// pixelHeight: 文字描画ピクセルサイズ
	// atlasWidth: テクスチャアトラスの幅
	// atlasHeight: テクスチャアトラスの高さ
	bool Initialize(Renderer* renderer, const std::string& fontPath, float pixelHeight = 32.0f,
	                uint32_t atlasWidth = 2048, uint32_t atlasHeight = 2048);

	// 指定コードポイントのグリフ情報を取得する
	// まだキャッシュにない場合、フォントからラスタライズしてアトラスに追加する
	const CachedGlyph* GetGlyph(uint32_t codepoint);

	// テクスチャアトラスの GPU SRV ハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetAtlasSrvGpu() const { return atlasSrvGpu_; }
	// テクスチャアトラスの CPU SRV ハンドル (非ShaderVisible, コピー元として使用)
	D3D12_CPU_DESCRIPTOR_HANDLE GetAtlasSrvCpuMaster() const { return atlasSrvCpuMaster_; }

	// フォントのアセンダーを取得 (ベースラインから上端まで)
	int GetAscent() const { return ascent_; }
	int GetDescent() const { return descent_; }
	int GetLineHeight() const { return ascent_ - descent_ + lineGap_; }

	float GetPixelHeight() const { return pixelHeight_; }

	bool IsInitialized() const { return initialized_; }

private:
	// アトラスの空き領域に新しいグリフを書き込む
	bool AddGlyphToAtlas(uint32_t codepoint, CachedGlyph& outGlyph);

	// CPUバッファの内容をGPUテクスチャに転送する
	void UploadAtlasRegion(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t* data);

private:
	Renderer* renderer_ = nullptr;
	Font font_;
	float pixelHeight_ = 32.0f;

	uint32_t atlasWidth_ = 2048;
	uint32_t atlasHeight_ = 2048;

	// CPU側のアトラスデータ (R8_UNORM → RGBA8に展開して転送)
	std::vector<uint8_t> cpuAtlas_;

	// 現在の書き込み位置 (シンプルな行ベースのパッキング)
	uint32_t cursorX_ = 0;
	uint32_t cursorY_ = 0;
	uint32_t rowHeight_ = 0; // 現在の行の最大高さ

	// GPU リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture_;
	D3D12_CPU_DESCRIPTOR_HANDLE atlasSrvCpu_{};
	D3D12_CPU_DESCRIPTOR_HANDLE atlasSrvCpuMaster_{};
	D3D12_GPU_DESCRIPTOR_HANDLE atlasSrvGpu_{};

	// キャッシュ: コードポイント → グリフ情報
	std::unordered_map<uint32_t, CachedGlyph> cache_;

	// フォントメトリクス
	int ascent_ = 0;
	int descent_ = 0;
	int lineGap_ = 0;

	bool initialized_ = false;
};

} // namespace Engine
