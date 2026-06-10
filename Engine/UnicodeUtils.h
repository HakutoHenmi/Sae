// Engine/UnicodeUtils.h
// UTF-8 文字列から Unicode コードポイントへの変換ユーティリティ
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {

// UTF-8 文字列から次の1文字（コードポイント）を取得する
// str: UTF-8 文字列の先頭ポインタ
// end: 文字列の終端ポインタ
// outCodepoint: 取得したコードポイントを格納
// 戻り値: 次の文字の先頭ポインタ
inline const char* Utf8NextCodepoint(const char* str, const char* end, uint32_t& outCodepoint) {
	if (str >= end) {
		outCodepoint = 0;
		return end;
	}

	uint8_t c = static_cast<uint8_t>(*str);

	// ASCII (1バイト: 0xxxxxxx)
	if (c < 0x80) {
		outCodepoint = c;
		return str + 1;
	}

	// 2バイト (110xxxxx 10xxxxxx)
	if ((c & 0xE0) == 0xC0) {
		if (str + 1 >= end) { outCodepoint = 0xFFFD; return end; }
		outCodepoint = (c & 0x1F) << 6;
		outCodepoint |= (static_cast<uint8_t>(str[1]) & 0x3F);
		return str + 2;
	}

	// 3バイト (1110xxxx 10xxxxxx 10xxxxxx)
	if ((c & 0xF0) == 0xE0) {
		if (str + 2 >= end) { outCodepoint = 0xFFFD; return end; }
		outCodepoint = (c & 0x0F) << 12;
		outCodepoint |= (static_cast<uint8_t>(str[1]) & 0x3F) << 6;
		outCodepoint |= (static_cast<uint8_t>(str[2]) & 0x3F);
		return str + 3;
	}

	// 4バイト (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
	if ((c & 0xF8) == 0xF0) {
		if (str + 3 >= end) { outCodepoint = 0xFFFD; return end; }
		outCodepoint = (c & 0x07) << 18;
		outCodepoint |= (static_cast<uint8_t>(str[1]) & 0x3F) << 12;
		outCodepoint |= (static_cast<uint8_t>(str[2]) & 0x3F) << 6;
		outCodepoint |= (static_cast<uint8_t>(str[3]) & 0x3F);
		return str + 4;
	}

	// 不正バイト
	outCodepoint = 0xFFFD;
	return str + 1;
}

// UTF-8 文字列を全コードポイントのベクタに変換
inline std::vector<uint32_t> Utf8ToCodepoints(const std::string& utf8) {
	std::vector<uint32_t> result;
	result.reserve(utf8.size()); // 最大でもバイト数
	const char* ptr = utf8.data();
	const char* end = ptr + utf8.size();
	while (ptr < end) {
		uint32_t cp;
		ptr = Utf8NextCodepoint(ptr, end, cp);
		if (cp != 0) {
			result.push_back(cp);
		}
	}
	return result;
}

} // namespace Engine
