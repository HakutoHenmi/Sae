#include "Audio.h"
#include "PathUtils.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace Engine {

Audio* Audio::instance_ = nullptr;

Audio::Audio() {}
Audio::~Audio() { Shutdown(); }

Audio* Audio::GetInstance() { return instance_; }

bool Audio::Initialize() {
	instance_ = this; // インスタンス登録

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(XAudio2Create(&xa_, 0, XAUDIO2_DEFAULT_PROCESSOR)))
		return false;

	if (FAILED(xa_->CreateMasteringVoice(&master_)))
		return false;

	hr = MFStartup(MF_VERSION);
	if (FAILED(hr))
		return false;

	return true;
}

void Audio::Shutdown() {
	// 全ボイス停止・破棄
	for (auto& pair : activeVoices_) {
		if (pair.second.source) {
			pair.second.source->Stop();
			pair.second.source->DestroyVoice();
		}
	}
	activeVoices_.clear();

	if (master_) {
		master_->DestroyVoice();
		master_ = nullptr;
	}
	xa_.Reset();
	MFShutdown();
	CoUninitialize();

	instance_ = nullptr;
}

// std::string 版 Load
uint32_t Audio::Load(const std::string& path) {
	// ★修正: GetUnifiedPath を使用して絶対パスを解決し、ワイド文字に変換
	std::wstring wpath = Engine::PathUtils::FromUTF8(Engine::PathUtils::GetUnifiedPath(path));

	SoundData newSound = {};
	if (LoadViaMF(wpath, newSound)) {
		soundDatas_.push_back(newSound);
		return (uint32_t)(soundDatas_.size() - 1);
	}
	return 0xFFFFFFFF; // エラー
}

// ハンドル指定再生
size_t Audio::Play(uint32_t soundHandle, bool loop, float volume) {
	// 定期お掃除 (再生終わったボイスを消す)
	GarbageCollect();

	if (soundHandle >= soundDatas_.size())
		return 0;

	const auto& sd = soundDatas_[soundHandle];
	IXAudio2SourceVoice* src = nullptr;

	if (FAILED(xa_->CreateSourceVoice(&src, &sd.wfx))) {
		return 0;
	}

	src->SetVolume(volume);

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = sd.data.data();
	buf.AudioBytes = (UINT32)sd.data.size();
	buf.Flags = XAUDIO2_END_OF_STREAM;
	if (loop) {
		buf.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	src->SubmitSourceBuffer(&buf);
	src->Start();

	size_t handle = nextVoiceHandle_++;
	VoiceData vd;
	vd.source = src;
	vd.isLoop = loop;
	activeVoices_[handle] = vd;

	return handle;
}

void Audio::Stop(size_t voiceHandle) {
	auto it = activeVoices_.find(voiceHandle);
	if (it != activeVoices_.end()) {
		if (it->second.source) {
			it->second.source->Stop();
			it->second.source->FlushSourceBuffers();
			it->second.source->DestroyVoice();
		}
		activeVoices_.erase(it);
	}
}

void Audio::SetVolume(size_t voiceHandle, float volume) {
	auto it = activeVoices_.find(voiceHandle);
	if (it != activeVoices_.end() && it->second.source) {
		it->second.source->SetVolume(volume);
	}
}

void Audio::StopAll() {
	for(auto& pair : activeVoices_) {
		if(pair.second.source) {
			pair.second.source->Stop();
			pair.second.source->FlushSourceBuffers();
			pair.second.source->DestroyVoice();
		}
	}
	activeVoices_.clear();
}

void Audio::GarbageCollect() {
	// 再生が終了している非ループボイスを削除
	for (auto it = activeVoices_.begin(); it != activeVoices_.end();) {
		if (!it->second.isLoop) {
			XAUDIO2_VOICE_STATE state;
			it->second.source->GetState(&state);
			if (state.BuffersQueued == 0) { // 再生完了
				it->second.source->DestroyVoice();
				it = activeVoices_.erase(it);
				continue;
			}
		}
		++it;
	}
}

// MediaFoundationを使ったロード実装
bool Audio::LoadViaMF(const std::wstring& path, SoundData& outData) {
	HRESULT hr;
	Microsoft::WRL::ComPtr<IMFSourceReader> reader;
	hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
	if (FAILED(hr))
		return false;

	hr = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	if (FAILED(hr))
		return false;
	hr = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
	if (FAILED(hr))
		return false;

	Microsoft::WRL::ComPtr<IMFMediaType> mediaType;
	MFCreateMediaType(&mediaType);
	mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, mediaType.Get());
	if (FAILED(hr))
		return false;

	Microsoft::WRL::ComPtr<IMFMediaType> outputMediaType;
	reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputMediaType);

	WAVEFORMATEX* pWfx = nullptr;
	UINT32 size = 0;
	MFCreateWaveFormatExFromMFMediaType(outputMediaType.Get(), &pWfx, &size);
	if (pWfx) {
		outData.wfx = *pWfx;
		CoTaskMemFree(pWfx);
	}

	while (true) {
		Microsoft::WRL::ComPtr<IMFSample> sample;
		DWORD flags = 0;
		hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
		if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
			break;

		if (sample) {
			Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
			sample->ConvertToContiguousBuffer(&buffer);
			BYTE* pAudioData = nullptr;
			DWORD cbCurrentLength = 0;
			if (SUCCEEDED(buffer->Lock(&pAudioData, nullptr, &cbCurrentLength))) {
				size_t oldSize = outData.data.size();
				outData.data.resize(oldSize + cbCurrentLength);
				memcpy(outData.data.data() + oldSize, pAudioData, cbCurrentLength);
				buffer->Unlock();
			}
		}
	}
	return true;
}

} // namespace Engine