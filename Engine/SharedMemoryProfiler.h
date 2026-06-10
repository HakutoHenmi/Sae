#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace Engine {

// 共有メモリに配置するデータ構造（C#側と同じレイアウトにする必要があります）
#pragma pack(push, 4)
struct SharedEngineData {
    uint32_t frameNumber;
    float fps;
    float deltaTime;
    uint32_t drawCalls;
    uint32_t particleCount;
    uint32_t lightCount;
    float playerX;
    float playerY;
    float playerZ;
    
    // プロ向け追加データ
    float cpuLogicTimeMs;
    float gpuRenderTimeMs;
    float systemRamUsageMB;
    float videoRamUsageMB;
    char eventMarker[64];
};
#pragma pack(pop)

class SharedMemoryProfiler {
public:
    static SharedMemoryProfiler& GetInstance() {
        static SharedMemoryProfiler instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();
    
    // 毎フレームの終わりに呼び出して、共有メモリにデータを書き込む
    void CommitFrame();

    // 各種データ設定用API
    void SetFPS(float fps) { data_.fps = fps; }
    void SetDeltaTime(float dt) { data_.deltaTime = dt; }
    void SetDrawCalls(uint32_t count) { data_.drawCalls = count; }
    void SetParticleCount(uint32_t count) { data_.particleCount = count; }
    void SetLightCount(uint32_t count) { data_.lightCount = count; }
    void SetPlayerPos(float x, float y, float z) {
        data_.playerX = x;
        data_.playerY = y;
        data_.playerZ = z;
    }

    void SetCpuLogicTime(float ms) { data_.cpuLogicTimeMs = ms; }
    void SetGpuRenderTime(float ms) { data_.gpuRenderTimeMs = ms; }
    void SetSystemRamUsage(float mb) { data_.systemRamUsageMB = mb; }
    void SetVideoRamUsage(float mb) { data_.videoRamUsageMB = mb; }
    
    // イベントマーカー（63文字まで）
    void SetEventMarker(const char* marker) {
        if (marker) {
            strncpy_s(data_.eventMarker, marker, sizeof(data_.eventMarker) - 1);
            data_.eventMarker[sizeof(data_.eventMarker) - 1] = '\0';
        } else {
            data_.eventMarker[0] = '\0';
        }
    }

    SharedEngineData& GetData() { return data_; }

private:
    SharedMemoryProfiler() = default;
    ~SharedMemoryProfiler() { Shutdown(); }

    HANDLE hMapFile_ = nullptr;
    SharedEngineData* pBuf_ = nullptr;
    SharedEngineData data_ = {};
};

} // namespace Engine
