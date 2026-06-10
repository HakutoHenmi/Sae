#pragma once
#include "System.h"
#include "Coordinator.h"
#include "Renderer.h"
#include <memory>

namespace TDEngine {
namespace ECS {

    class RenderSystem : public System {
    public:
        // Coordinatorへの参照を受け取って初期化する
        void Init(Coordinator* coordinator);

        // 毎フレームの描画処理（Rendererの描画キューへの登録）
        void Draw();

    private:
        Coordinator* m_coordinator = nullptr;
    };

} // namespace ECS
} // namespace TDEngine
