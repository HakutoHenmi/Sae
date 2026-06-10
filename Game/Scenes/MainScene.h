#pragma once
#include "IScene.h"
#include "../../Engine/Camera.h"
#include "../../Engine/Renderer.h"
#include <memory>
#include <chrono>

#include "../../Engine/EventSystem.h"
#include "../../externals/entt/entt.hpp"

#include "../Systems/ISystem.h" // For GameContext
#include "../../Engine/ParticleEditor.h"
#include <vector>
#include <string>
#include <DirectXMath.h>

namespace Game {

class MainScene : public Engine::IScene {
public:
    MainScene() = default;
    ~MainScene() override = default;

    void Initialize(Engine::WindowDX* dx, const Engine::SceneParameters& params) override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;

    entt::registry& GetRegistry() { return registry_; }
    Engine::EventSystem& GetEventSystem() { return eventSystem_; }

    std::vector<entt::entity>& GetSelectedEntities() { return selectedEntities_; }
    entt::entity GetSelectedEntity() const { return selectedEntity_; }
    void SetSelectedEntity(entt::entity e) { selectedEntity_ = e; }
    
    entt::entity CreateEntity(const std::string& name = "Entity");
    void DestroyObject(uint32_t id);
    
    bool GetIsPlaying() const { return isPlaying_; }
    void SetIsPlaying(bool b) { isPlaying_ = b; }
    
    Engine::ParticleEditor& GetParticleEditor() { return particleEditor_; }
    Engine::Camera& GetCamera() { return camera_; }
    GameContext& GetContext() { return context_; }
    
    Engine::Matrix4x4 GetWorldMatrix(int /*index*/) const { Engine::Matrix4x4 m; return m; }
    void SyncTag(entt::entity /*e*/) {}

private:
    Engine::Camera camera_;
    Engine::WindowDX* dx_ = nullptr;
    Engine::Renderer* renderer_ = nullptr;
    
    entt::registry registry_;
    Engine::EventSystem eventSystem_;
    
    std::chrono::steady_clock::time_point lastTime_;
    float dt_ = 1.0f / 60.0f;
    float totalTime_ = 0.0f;

    std::vector<entt::entity> selectedEntities_;
    entt::entity selectedEntity_ = entt::null;
    bool isPlaying_ = false;
    Engine::ParticleEditor particleEditor_;
    GameContext context_;

    // マスコットモード状態管理フラグ
    bool isMascotMode_ = true;

    // トランジション（チカチカ防止）用
    int transitionTimer_ = 0;
    bool nextMascotMode_ = true;
    uint32_t whiteTex_ = 0;

    // ドラッグ・位置記憶用
    bool isDraggingWindow_ = false;
    POINT lastCursorPos_{};
    POINT startCursorPos_{};

    // --- Boids (Stardust Swarm) 用データ ---
    struct Boid {
        Engine::Vector3 position;
        Engine::Vector3 velocity;
        Engine::Vector4 color;
        float scale;
        float angle; // 表示用回転角
    };

    std::vector<Boid> boids_;
    
    // Boidsパラメータ
    float boidSpeed_ = 60.0f;         // ゆっくり漂うように
    float boidPerception_ = 200.0f;   // 広く感知する
    float boidSeparation_ = 80.0f;    // 分離の距離を広げる
    float forceSeparation_ = 3.0f;    // 分離の力を強くして密集を防ぐ
    float forceAlignment_ = 1.0f;
    float forceCohesion_ = 0.3f;      // 結合の力を弱めてフワッとさせる
    float forceMouseEvade_ = 5.0f;
    float mouseEvadeRadius_ = 250.0f;

    // 中心への引力
    Engine::Vector3 swarmCenter_{ 640.0f, 360.0f, 0.0f };

    // --- フェーズ1: ポモドーロ機能用データ ---
    float pomodoroTimer_ = 0.0f;
    bool isPomodoroWork_ = true; // true: 25分(作業), false: 5分(休憩)
    float pomodoroTransition_ = 0.0f; // 0.0(作業色) 〜 1.0(休憩色)
    Engine::Vector4 currentColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
};

} // namespace Game
