// Engine/Font.h
// フォントの読み込みと文字のラスタライズを担当
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// stb_truetype の構造体を前方宣言相当で使うため
struct stbtt_fontinfo;

namespace Engine {

// 1文字のグリフ情報
struct GlyphMetrics {
	int width = 0;       // ビットマップ幅 (ピクセル)
	int height = 0;      // ビットマップ高さ (ピクセル)
	int bearingX = 0;    // ベースラインからの横オフセット
	int bearingY = 0;    // ベースラインからの縦オフセット (上方向に正)
	int advance = 0;     // 次の文字までの横移動量 (ピクセル)
};

class Font {
public:
	Font() = default;
	~Font();

	Font(const Font&) = delete;
	Font& operator=(const Font&) = delete;

	// フォントファイル (.ttf / .otf) を読み込む
	bool Load(const std::string& filePath);

	// 指定のコードポイントのグリフをラスタライズし、8bitグレースケール画像を返す
	// outMetrics: グリフの配置情報
	// pixelHeight: 描画するピクセルサイズ
	// 戻り値: グレースケールビットマップデータ (width * height バイト)
	std::vector<uint8_t> RasterizeGlyph(uint32_t codepoint, float pixelHeight, GlyphMetrics& outMetrics) const;

	// フォントの行間情報を取得
	void GetVerticalMetrics(float pixelHeight, int& outAscent, int& outDescent, int& outLineGap) const;

	bool IsLoaded() const { return fontInfo_ != nullptr; }

private:
	std::vector<uint8_t> fontData_; // ファイルデータ（メモリ上に保持）
	stbtt_fontinfo* fontInfo_ = nullptr;
};

} // namespace Engine
