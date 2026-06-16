#include "AudioLoopbackCapture.h"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <initguid.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>

namespace Game {

AudioLoopbackCapture::AudioLoopbackCapture() {
    sampleBuffer_.resize(maxSamples_, 0.0f);
}

AudioLoopbackCapture::~AudioLoopbackCapture() {
    Shutdown();
}

void AudioLoopbackCapture::Initialize() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&AudioLoopbackCapture::CaptureLoop, this);
}

void AudioLoopbackCapture::Shutdown() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void AudioLoopbackCapture::GetSamples(std::vector<float>& outSamples) {
    std::lock_guard<std::mutex> lock(mutex_);
    outSamples = sampleBuffer_;
}

void AudioLoopbackCapture::CaptureLoop() {
    // COMの初期化
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        char buf[256]; sprintf_s(buf, "CoInitializeEx Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK);
        running_ = false;
        return;
    }

    while (running_) {
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* audioClient = nullptr;
        IAudioCaptureClient* captureClient = nullptr;
        WAVEFORMATEX* pwfx = nullptr;

        auto Cleanup = [&]() {
            if (pwfx) CoTaskMemFree(pwfx);
            if (captureClient) captureClient->Release();
            if (audioClient) audioClient->Release();
            if (device) device->Release();
            if (enumerator) enumerator->Release();
        };

        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&enumerator);
        if (FAILED(hr)) {
            static bool reported = false;
            if (!reported) { char buf[256]; sprintf_s(buf, "CoCreateInstance(Enumerator) Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
            running_ = false;
            return;
        }

        // 指定されたターゲットデバイスがあればそれを試す
        if (!targetDeviceId_.empty()) {
            hr = enumerator->GetDevice(targetDeviceId_.c_str(), &device);
            if (FAILED(hr)) {
                targetDeviceId_ = L""; // 失敗時はクリアしてフォールバックへ
            }
        }

        // デフォルトのオーディオ出力デバイス（スピーカー）を取得
        if (targetDeviceId_.empty()) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
            if (FAILED(hr)) {
                hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
            }
            if (FAILED(hr)) {
                static bool reported = false;
                if (!reported) { char buf[256]; sprintf_s(buf, "GetDefaultAudioEndpoint Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
                enumerator->Release();
                Sleep(250);
                continue;
            }
            if (SUCCEEDED(hr)) {
                LPWSTR id = nullptr;
                if (SUCCEEDED(device->GetId(&id))) {
                    targetDeviceId_ = id;
                    CoTaskMemFree(id);
                }
            }
        }
        if (FAILED(hr)) {
            Cleanup();
            Sleep(250);
            continue;
        }

        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient);
        if (FAILED(hr)) {
            static bool reported = false;
            if (!reported) { char buf[256]; sprintf_s(buf, "Activate Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
            Cleanup();
            Sleep(250);
            continue;
        }

        hr = audioClient->GetMixFormat(&pwfx);
        if (FAILED(hr)) {
            static bool reported = false;
            if (!reported) { char buf[256]; sprintf_s(buf, "GetMixFormat Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
            Cleanup();
            Sleep(250);
            continue;
        }

        // デバイスの最小周期を取得してバッファサイズを最適化する
        REFERENCE_TIME defaultPeriod = 0;
        REFERENCE_TIME minimumPeriod = 0;
        hr = audioClient->GetDevicePeriod(&defaultPeriod, &minimumPeriod);
        if (FAILED(hr)) {
            defaultPeriod = 1000000; // 100ms
        }

        // ループバックキャプチャの初期化
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif
        // バッファサイズには公式仕様に従い 0 を指定しなければならない (ループバックの場合)
        // AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM や SRC_DEFAULT_QUALITY は LOOPBACK と併用不可のため外す
        hr = audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            0,
            0,
            pwfx,
            NULL);
            
        // 一部の環境・ドライバではバッファサイズ0が拒否されるため、1秒分のバッファでフォールバックを試みる
        if (FAILED(hr)) {
            hr = audioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_LOOPBACK,
                10000000, // 1秒
                0,
                pwfx,
                NULL);
        }

        if (FAILED(hr)) {
            static bool reported = false;
            if (!reported) { char buf[256]; sprintf_s(buf, "Initialize Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
            Cleanup();
            Sleep(250);
            continue;
        }

        hr = audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&captureClient);
        if (FAILED(hr)) {
            static bool reported = false;
            if (!reported) { char buf[256]; sprintf_s(buf, "GetService Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
            Cleanup();
            Sleep(250);
            continue;
        }

        hr = audioClient->Start();
        if (FAILED(hr)) {
            static bool reported = false;
            if (!reported) { char buf[256]; sprintf_s(buf, "Start Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
            Cleanup();
            Sleep(250);
            continue;
        }

        bool deviceActive = true;
        UINT32 packetLength = 0;
        std::vector<float> localBuffer(maxSamples_, 0.0f);

        DWORD lastDataTime = GetTickCount(); // 音声データが最後に取得できた時間

        while (running_ && deviceActive) {
            // CPU使用率よりもバッファドロップ防止を最優先し、超高速ポーリングを行う
            Sleep(1);

            // 定期的にすべてのアクティブなデバイスをスキャンし、音が鳴っているデバイスを自動で追跡する
            DWORD now = GetTickCount();
            static DWORD lastCheckTime = GetTickCount();
            if (now - lastCheckTime > 2000) {
                lastCheckTime = now;
                
                IMMDeviceCollection* pDevices = nullptr;
                HRESULT enumHr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
                if (SUCCEEDED(enumHr)) {
                    UINT count = 0;
                    pDevices->GetCount(&count);
                    
                    std::wstring bestDeviceId = targetDeviceId_;
                    float maxPeak = 0.0f;

                    for (UINT i = 0; i < count; ++i) {
                        IMMDevice* pEndpoint = nullptr;
                        if (SUCCEEDED(pDevices->Item(i, &pEndpoint))) {
                            IAudioMeterInformation* pMeterInfo = nullptr;
                            if (SUCCEEDED(pEndpoint->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo))) {
                                float peak = 0.0f;
                                pMeterInfo->GetPeakValue(&peak);
                                if (peak > maxPeak) {
                                    maxPeak = peak;
                                    LPWSTR id = nullptr;
                                    if (SUCCEEDED(pEndpoint->GetId(&id))) {
                                        bestDeviceId = id;
                                        CoTaskMemFree(id);
                                    }
                                }
                                pMeterInfo->Release();
                            }
                            pEndpoint->Release();
                        }
                    }
                    pDevices->Release();

                    // 微小なノイズ（Phantom Peak）を無視するための高い閾値 (0.01f = 1%音量)
                    if (maxPeak > 0.01f && bestDeviceId != targetDeviceId_) {
                        targetDeviceId_ = bestDeviceId;
                        deviceActive = false;
                        break;
                    }
                    
                    // どのデバイスからも有意な音が鳴っていない場合、システムのデフォルトに戻す
                    if (maxPeak <= 0.01f) {
                        IMMDevice* currentDefaultDevice = nullptr;
                        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &currentDefaultDevice))) {
                            LPWSTR defaultId = nullptr;
                            if (SUCCEEDED(currentDefaultDevice->GetId(&defaultId))) {
                                if (targetDeviceId_ != defaultId) {
                                    targetDeviceId_ = defaultId;
                                    deviceActive = false;
                                }
                                CoTaskMemFree(defaultId);
                            }
                            currentDefaultDevice->Release();
                        }
                    }
                }
            }

            if (!deviceActive) break;

            hr = captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                static bool reported = false;
                if (!reported) { char buf[256]; sprintf_s(buf, "GetNextPacketSize Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
                // デバイスが無効になったか失われた場合、外側のループに戻って再接続を試みる
                deviceActive = false;
                break;
            }

            if (packetLength > 0) {
                lastDataTime = GetTickCount();
            } else {
                // 5秒間ずっとパケットが来ない場合は、デバイスがスタンバイ状態のままスタックしているとみなして再接続を試みる
                if (GetTickCount() - lastDataTime > 5000) {
                    deviceActive = false;
                    break;
                }
            }

            // --- STATUS LOGGING ---
            static DWORD lastStatusTime = 0;
            if (GetTickCount() - lastStatusTime > 2000) {
                lastStatusTime = GetTickCount();
                FILE* fp = nullptr;
                fopen_s(&fp, "wasapi_status.txt", "w"); // 上書きで常に最新状態を出力
                if (fp) {
                    fprintf(fp, "Thread Running. PacketLength: %u\n", packetLength);
                    
                    IMMDeviceCollection* pDevices = nullptr;
                    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices))) {
                        UINT count = 0;
                        pDevices->GetCount(&count);
                        for (UINT i = 0; i < count; ++i) {
                            IMMDevice* pEndpoint = nullptr;
                            if (SUCCEEDED(pDevices->Item(i, &pEndpoint))) {
                                IAudioMeterInformation* pMeterInfo = nullptr;
                                if (SUCCEEDED(pEndpoint->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo))) {
                                    float peak = 0.0f;
                                    pMeterInfo->GetPeakValue(&peak);
                                    
                                    LPWSTR id = nullptr;
                                    pEndpoint->GetId(&id);
                                    
                                    fprintf(fp, "Device %u Peak: %f, ID: %ws\n", i, peak, id ? id : L"Unknown");
                                    
                                    if (id) CoTaskMemFree(id);
                                    pMeterInfo->Release();
                                }
                                pEndpoint->Release();
                            }
                        }
                        pDevices->Release();
                    }
                    fclose(fp);
                }
            }
            // ----------------------

            while (packetLength > 0) {
                BYTE* data = nullptr;
                UINT32 numFramesRead = 0;
                DWORD flags = 0;

                hr = captureClient->GetBuffer(&data, &numFramesRead, &flags, NULL, NULL);
                if (FAILED(hr)) {
                    static bool reported = false;
                    if (!reported) { char buf[256]; sprintf_s(buf, "GetBuffer Failed: %08X", hr); MessageBoxA(NULL, buf, "WASAPI Error", MB_OK); reported = true; }
                    // バッファ取得に失敗した場合は、ストリームが破損したとみなし、
                    // 無限ループを防ぐために問答無用でデバイスを再接続する
                    deviceActive = false;
                    break;
                }

                // --- DIAGNOSTIC LOGGING ---
                static DWORD lastLogTime = 0;
                if (GetTickCount() - lastLogTime > 1000) {
                    lastLogTime = GetTickCount();
                    FILE* fp = nullptr;
                    fopen_s(&fp, "wasapi_log.txt", "a");
                    if (fp) {
                        fprintf(fp, "frames: %u, flags: %u, format: %u, bits: %u\n", numFramesRead, flags, pwfx->wFormatTag, pwfx->wBitsPerSample);
                        fclose(fp);
                    }
                }
                // --------------------------

                int channels = pwfx->nChannels;
                bool isFloat = false;

                if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                    isFloat = true;
                } else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                    WAVEFORMATEXTENSIBLE* pEx = (WAVEFORMATEXTENSIBLE*)pwfx;
                    if (pEx->SubFormat.Data1 == 0x00000003 &&
                        pEx->SubFormat.Data2 == 0x0000 &&
                        pEx->SubFormat.Data3 == 0x0010 &&
                        pEx->SubFormat.Data4[0] == 0x80 &&
                        pEx->SubFormat.Data4[1] == 0x00 &&
                        pEx->SubFormat.Data4[2] == 0x00 &&
                        pEx->SubFormat.Data4[3] == 0xaa &&
                        pEx->SubFormat.Data4[4] == 0x00 &&
                        pEx->SubFormat.Data4[5] == 0x38 &&
                        pEx->SubFormat.Data4[6] == 0x9b &&
                        pEx->SubFormat.Data4[7] == 0x71) {
                        isFloat = true;
                    }
                }

                if (numFramesRead > 0 && data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    for (UINT32 i = 0; i < numFramesRead; ++i) {
                        float sample = 0.0f;
                        
                        if (isFloat && pwfx->wBitsPerSample == 32) {
                            // 32-bit Float
                            float* fData = (float*)data;
                            for (int c = 0; c < channels; ++c) {
                                sample += fData[i * channels + c];
                            }
                            sample /= channels;
                        } else if (!isFloat && pwfx->wBitsPerSample == 16) {
                            // 16-bit PCM
                            int16_t* sData = (int16_t*)data;
                            for (int c = 0; c < channels; ++c) {
                                sample += sData[i * channels + c] / 32768.0f;
                            }
                            sample /= channels;
                        } else if (!isFloat && pwfx->wBitsPerSample == 24) {
                            // 24-bit PCM (3 bytes per sample)
                            BYTE* bData = (BYTE*)data;
                            for (int c = 0; c < channels; ++c) {
                                int idx = (i * channels + c) * 3;
                                // 3バイトを結合して符号付き24bit整数にする
                                int32_t val = (bData[idx]) | (bData[idx + 1] << 8) | (bData[idx + 2] << 16);
                                // 24bit目の符号ビットが1なら、最上位8ビットを1で埋める（符号拡張）
                                if (val & 0x800000) {
                                    val |= 0xFF000000;
                                }
                                sample += (float)val / 8388608.0f; // 2^23 で割る
                            }
                            sample /= channels;
                        } else if (!isFloat && pwfx->wBitsPerSample == 32) {
                            // 32-bit PCM Integer
                            int32_t* iData = (int32_t*)data;
                            for (int c = 0; c < channels; ++c) {
                                sample += (float)iData[i * channels + c] / 2147483648.0f;
                            }
                            sample /= channels;
                        } else {
                            // Fallback to silent or float check
                            float* fData = (float*)data;
                            for (int c = 0; c < channels; ++c) {
                                sample += fData[i * channels + c];
                            }
                            sample /= channels;
                        }

                        // 万が一の計算誤差やNaN混入を防ぐ
                        if (std::isnan(sample)) sample = 0.0f;

                        // バッファをローテートしながら書き込む
                        localBuffer.erase(localBuffer.begin());
                        localBuffer.push_back(sample);
                    }
                } else if (flags & AUDCLNT_BUFFERFLAGS_SILENT || numFramesRead > 0) {
                    // 無音時は0を詰める
                    UINT32 fillCount = (numFramesRead > 0) ? numFramesRead : 128;
                    for (UINT32 i = 0; i < fillCount; ++i) {
                        localBuffer.erase(localBuffer.begin());
                        localBuffer.push_back(0.0f);
                    }
                }

                captureClient->ReleaseBuffer(numFramesRead);

                hr = captureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) {
                    deviceActive = false;
                    break;
                }
            }

            // 定期的にスレッドセーフにメインバッファへコピー
            {
                std::lock_guard<std::mutex> lock(mutex_);
                sampleBuffer_ = localBuffer;
            }
        }

        audioClient->Stop();
        Cleanup();
        
        if (running_ && !deviceActive) {
            // 再接続処理に移る前に少し待つ
            Sleep(250);
        }
    }

    CoUninitialize();
    running_ = false;
}

} // namespace Game
