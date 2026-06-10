#pragma once
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#undef max
#undef min
#include <wrl.h>
#include <xaudio2.h>
#include <algorithm>

// Media Foundation
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace Engine {

// 音声データ構造体
struct SoundData {
	std::vector<BYTE> data;
	WAVEFORMATEX wfx;
};

// 再生中のボイス情報
struct VoiceData {
	IXAudio2SourceVoice* source;
	bool isLoop;
};

class Audio {
public:
	// コンストラクタ・デストラクタ
	Audio();
	~Audio();

	// シングルトンインスタンス取得
	static Audio* GetInstance();

	// 初期化
	bool Initialize();

	// 終了処理
	void Shutdown();

	// 音声ファイル読み込み (mp3/wav対応)
	// 戻り値: 音声ハンドル (失敗時 -1/0xFFFFFFFF)
	uint32_t Load(const std::string& path);

	// 再生
	// 戻り値: 再生ハンドル (停止時に使用)
	// volume: 0.0f ~ 1.0f
	size_t Play(uint32_t soundHandle, bool loop = false, float volume = 1.0f);

	// 停止
	void Stop(size_t voiceHandle);

	// ★追加: ボイスの音量変更
	void SetVolume(size_t voiceHandle, float volume);

	// ★追加: マスター音量管理
	float GetMasterBGMVolume() const { return masterBGMVolume_; }
	void SetMasterBGMVolume(float volume) { masterBGMVolume_ = (std::max)(0.0f, (std::min)(1.0f, volume)); }
	float GetMasterSEVolume() const { return masterSEVolume_; }
	void SetMasterSEVolume(float volume) { masterSEVolume_ = (std::max)(0.0f, (std::min)(1.0f, volume)); }

	// ★追加: 全てのボイスを停止 (Playモード終了時用)
	void StopAll();

private:
	// 内部用ロード関数
	bool LoadViaMF(const std::wstring& path, SoundData& outData);

	// 終了したボイスのお掃除
	void GarbageCollect();

private:
	static Audio* instance_;

	Microsoft::WRL::ComPtr<IXAudio2> xa_;
	IXAudio2MasteringVoice* master_ = nullptr;

	// 読み込んだ音声データ (Indexがハンドルになる)
	std::vector<SoundData> soundDatas_;

	// 再生中のボイス管理
	// Key: 発行した再生ハンドル, Value: ボイス実体
	std::map<size_t, VoiceData> activeVoices_;
	size_t nextVoiceHandle_ = 1;

	// ★追加: マスター音量
	float masterBGMVolume_ = 1.0f;
	float masterSEVolume_ = 1.0f;
};

} // namespace Engine