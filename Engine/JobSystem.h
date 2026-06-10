#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <algorithm>

namespace Engine {

    class JobSystem {
    public:
        // システムの初期化（スレッドプールの構築）
        static void Initialize();

        // システムのシャットダウン（スレッドの待機・終了）
        static void Shutdown();

        // 単一のジョブを非同期実行する
        static void Execute(const std::function<void()>& job);

        // 指定された件数(jobCount)の処理を、groupSizeごとに分割して並列実行する
        static void Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t)>& job);

        // キューに積まれた全てのジョブが完了するまで待機する
        static void Wait();

    private:
        // ワーカー本体の処理ループ
        static void WorkerThread();

    private:
        static std::vector<std::thread> workers_;
        static std::queue<std::function<void()>> jobQueue_;
        static std::mutex queueMutex_;
        static std::condition_variable condition_;
        static bool stop_;
        static std::atomic<uint32_t> working_;
    };

} // namespace Engine
