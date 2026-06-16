#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SMTCMonitor.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <fstream>
#include <filesystem>
#include <Windows.h>

#pragma comment(lib, "runtimeobject.lib")

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

namespace Game {

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

SMTCMonitor::SMTCMonitor() {
}

SMTCMonitor::~SMTCMonitor() {
    Shutdown();
}

void SMTCMonitor::Initialize() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&SMTCMonitor::MonitorLoop, this);
}

void SMTCMonitor::Shutdown() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool SMTCMonitor::CheckUpdated(SMTCInfo& outInfo) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (updated_) {
        outInfo = currentInfo_;
        updated_ = false;
        return true;
    }
    return false;
}

void SMTCMonitor::MonitorLoop() {
    // スレッドごとにCOMを初期化
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
        // すでに初期化されている場合は無視
    }

    GlobalSystemMediaTransportControlsSessionManager manager = nullptr;
    try {
        manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
    } catch (...) {
        return;
    }

    if (!manager) return;

    while (running_) {
        try {
            GlobalSystemMediaTransportControlsSession session = manager.GetCurrentSession();
            if (session) {
                // 再生状態
                auto playbackInfo = session.GetPlaybackInfo();
                bool isPlaying = false;
                if (playbackInfo) {
                    auto status = playbackInfo.PlaybackStatus();
                    isPlaying = (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                }

                // 曲情報
                auto props = session.TryGetMediaPropertiesAsync().get();
                if (props) {
                    std::wstring title = props.Title().c_str();
                    std::wstring artist = props.Artist().c_str();

                    if (title != lastTitle_ || artist != lastArtist_) {
                        lastTitle_ = title;
                        lastArtist_ = artist;

                        SMTCInfo info;
                        info.title = WStringToString(title);
                        info.artist = WStringToString(artist);
                        info.isPlaying = isPlaying;
                        info.hasThumbnail = false;

                        // サムネイル画像の処理
                        IRandomAccessStreamReference thumbRef = props.Thumbnail();
                        if (thumbRef) {
                            try {
                                auto stream = thumbRef.OpenReadAsync().get();
                                if (stream) {
                                    auto size = stream.Size();
                                    if (size > 0) {
                                        Buffer buffer(static_cast<uint32_t>(size));
                                        auto readOp = stream.ReadAsync(buffer, static_cast<uint32_t>(size), InputStreamOptions::None);
                                        readOp.get();
                                        
                                        DataReader reader = DataReader::FromBuffer(buffer);
                                        std::vector<uint8_t> bytes(reader.UnconsumedBufferLength());
                                        if (!bytes.empty()) {
                                            reader.ReadBytes(bytes);

                                            std::filesystem::create_directories("Resources/Textures/UI");
                                            thumbIndex_ = (thumbIndex_ + 1) % 10;
                                            std::string path = "Resources/Textures/UI/now_playing_" + std::to_string(thumbIndex_) + ".png";
                                            std::ofstream ofs(path, std::ios::binary);
                                            if (ofs) {
                                                ofs.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                                                ofs.close();
                                                info.hasThumbnail = true;
                                                info.thumbnailPath = path;
                                            }
                                        }
                                    }
                                }
                            } catch (...) {
                                // サムネイル読み込みエラーは無視
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            currentInfo_ = info;
                            updated_ = true;
                        }
                    } else {
                        // 再生状態のみの更新チェック
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (currentInfo_.isPlaying != isPlaying) {
                            currentInfo_.isPlaying = isPlaying;
                            updated_ = true;
                        }
                    }
                }
            }
        } catch (...) {
            // 例外が発生した場合は無視して次のループへ
        }

        // 1秒待機
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace Game
