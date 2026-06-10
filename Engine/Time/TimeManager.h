#pragma once
#include <chrono>

namespace Engine {

class TimeManager {
public:
    static TimeManager& GetInstance() {
        static TimeManager instance;
        return instance;
    }

    void Update(float dt) {
        unscaledDt_ = dt;
        if (unscaledDt_ > 0.1f) unscaledDt_ = 0.1f; // Cap dt to avoid huge jumps

        // Hitstop logic
        if (hitStopTimer_ > 0.0f) {
            hitStopTimer_ -= unscaledDt_;
            scaledDt_ = 0.0f; // Freeze time
        } else {
            scaledDt_ = unscaledDt_ * timeScale_;
        }
    }

    float GetDeltaTime() const { return scaledDt_; }
    float GetUnscaledDeltaTime() const { return unscaledDt_; }

    void SetTimeScale(float scale) { timeScale_ = scale; }
    float GetTimeScale() const { return timeScale_; }

    // Hitstop: freeze for 'duration' seconds
    void SetHitstop(float duration) {
        hitStopTimer_ = duration;
    }

private:
    TimeManager() {
        prev_ = std::chrono::steady_clock::now();
    }

    std::chrono::steady_clock::time_point prev_;
    float unscaledDt_ = 0.0f;
    float scaledDt_ = 0.0f;
    float timeScale_ = 1.0f;
    float hitStopTimer_ = 0.0f;
};

} // namespace Engine
