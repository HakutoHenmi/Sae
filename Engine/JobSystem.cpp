#include "JobSystem.h"

namespace Engine {

    std::vector<std::thread> JobSystem::workers_;
    std::queue<std::function<void()>> JobSystem::jobQueue_;
    std::mutex JobSystem::queueMutex_;
    std::condition_variable JobSystem::condition_;
    bool JobSystem::stop_ = false;
    std::atomic<uint32_t> JobSystem::working_{ 0 };

    void JobSystem::Initialize() {
        stop_ = false;
        working_ = 0;

        // 利用可能な論理コア数を取得。メインスレッド分として1つ減らす。
        uint32_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4; // 取得失敗時のフォールバック
        }
        numThreads = std::max<uint32_t>(1, numThreads - 1);

        for (uint32_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back(WorkerThread);
        }
    }

    void JobSystem::Shutdown() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();

        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    void JobSystem::Execute(const std::function<void()>& job) {
        working_++;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            jobQueue_.push(job);
        }
        condition_.notify_one();
    }

    void JobSystem::Dispatch(uint32_t jobCount, uint32_t groupSize, const std::function<void(uint32_t)>& job) {
        if (jobCount == 0 || groupSize == 0) {
            return;
        }

        uint32_t groupCount = (jobCount + groupSize - 1) / groupSize;
        working_ += groupCount;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            for (uint32_t groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
                uint32_t start = groupIndex * groupSize;
                uint32_t end = std::min(start + groupSize, jobCount);

                jobQueue_.push([job, start, end]() {
                    for (uint32_t i = start; i < end; ++i) {
                        job(i);
                    }
                });
            }
        }
        condition_.notify_all();
    }

    void JobSystem::Wait() {
        // 全てのタスクが完了するまで待機する
        while (working_ > 0) {
            std::this_thread::yield();
        }
    }

    void JobSystem::WorkerThread() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                condition_.wait(lock, [] { return stop_ || !jobQueue_.empty(); });

                // 終了フラグが立っており、かつキューが空ならスレッドを終了
                if (stop_ && jobQueue_.empty()) {
                    return;
                }

                job = std::move(jobQueue_.front());
                jobQueue_.pop();
            }

            // ジョブの実行
            job();
            working_--;
        }
    }

} // namespace Engine
