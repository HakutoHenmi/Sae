#pragma once
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <Windows.h>

namespace Game {

class AudioLoopbackCapture {
public:
    AudioLoopbackCapture();
    ~AudioLoopbackCapture();

    void Initialize();
    void Shutdown();

    // 最新のオーディオサンプル（振幅）を取得する
    void GetSamples(std::vector<float>& outSamples);
    bool IsRunning() const { return running_; }

private:
    void CaptureLoop();

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex mutex_;
    std::vector<float> sampleBuffer_;
    size_t maxSamples_ = 256; // 描画用に256サンプルあれば十分
    std::wstring targetDeviceId_; // 現在キャプチャしている対象デバイスのID
};

} // namespace Game
