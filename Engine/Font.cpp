// Engine/Font.cpp
#include "Font.h"
#include "../externals/stb/stb_truetype.h"

#include <cassert>
#include <cmath>
#include <fstream>

namespace Engine {

Font::~Font() {
	delete fontInfo_;
	fontInfo_ = nullptr;
}

bool Font::Load(const std::string& filePath) {
	// バイナリモードでフォントファイルを読み込む
	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return false;
	}

	const auto fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	fontData_.resize(static_cast<size_t>(fileSize));
	if (!file.read(reinterpret_cast<char*>(fontData_.data()), fileSize)) {
		fontData_.clear();
		return false;
	}
	file.close();

	// stb_truetype の初期化
	fontInfo_ = new stbtt_fontinfo();
	if (!stbtt_InitFont(fontInfo_, fontData_.data(), stbtt_GetFontOffsetForIndex(fontData_.data(), 0))) {
		delete fontInfo_;
		fontInfo_ = nullptr;
		fontData_.clear();
		return false;
	}

	return true;
}

std::vector<uint8_t> Font::RasterizeGlyph(uint32_t codepoint, float pixelHeight, GlyphMetrics& outMetrics) const {
	if (!fontInfo_) {
		outMetrics = {};
		return {};
	}

	float scale = stbtt_ScaleForPixelHeight(fontInfo_, pixelHeight);

	int ix0, iy0, ix1, iy1;
	stbtt_GetCodepointBitmapBox(fontInfo_, static_cast<int>(codepoint), scale, scale, &ix0, &iy0, &ix1, &iy1);

	int w = ix1 - ix0;
	int h = iy1 - iy0;

	if (w <= 0 || h <= 0) {
		// スペースなどの見えない文字
		int advanceWidth, leftBearing;
		stbtt_GetCodepointHMetrics(fontInfo_, static_cast<int>(codepoint), &advanceWidth, &leftBearing);
		outMetrics.width = 0;
		outMetrics.height = 0;
		outMetrics.bearingX = static_cast<int>(std::round(leftBearing * scale));
		outMetrics.bearingY = 0;
		outMetrics.advance = static_cast<int>(std::round(advanceWidth * scale));
		return {};
	}

	std::vector<uint8_t> bitmap(static_cast<size_t>(w * h), 0);
	stbtt_MakeCodepointBitmap(fontInfo_, bitmap.data(), w, h, w, scale, scale, static_cast<int>(codepoint));

	int advanceWidth, leftBearing;
	stbtt_GetCodepointHMetrics(fontInfo_, static_cast<int>(codepoint), &advanceWidth, &leftBearing);

	outMetrics.width = w;
	outMetrics.height = h;
	outMetrics.bearingX = ix0;
	outMetrics.bearingY = -iy0; // stb_truetype は上方向が負なので符号を反転
	outMetrics.advance = static_cast<int>(std::round(advanceWidth * scale));

	return bitmap;
}

void Font::GetVerticalMetrics(float pixelHeight, int& outAscent, int& outDescent, int& outLineGap) const {
	if (!fontInfo_) {
		outAscent = 0;
		outDescent = 0;
		outLineGap = 0;
		return;
	}

	float scale = stbtt_ScaleForPixelHeight(fontInfo_, pixelHeight);

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(fontInfo_, &ascent, &descent, &lineGap);

	outAscent = static_cast<int>(std::round(ascent * scale));
	outDescent = static_cast<int>(std::round(descent * scale));
	outLineGap = static_cast<int>(std::round(lineGap * scale));
}

} // namespace Engine
