#pragma once
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

namespace Game {

struct SMTCInfo {
    std::string title;
    std::string artist;
    bool hasThumbnail = false;
    std::string thumbnailPath;
    bool isPlaying = false;
};

class SMTCMonitor {
public:
    SMTCMonitor();
    ~SMTCMonitor();

    void Initialize();
    void Shutdown();

    // 更新されたかどうかをチェック。更新されたらtrueを返し、infoを書き換える
    bool CheckUpdated(SMTCInfo& outInfo);

private:
    void MonitorLoop();

    std::thread thread_;
    std::atomic<bool> running_{false};

    std::mutex mutex_;
    SMTCInfo currentInfo_;
    bool updated_{false};
    
    std::wstring lastTitle_;
    std::wstring lastArtist_;
    int thumbIndex_ = 0;
};

} // namespace Game
