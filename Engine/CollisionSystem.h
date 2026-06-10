#pragma once
#include "System.h"
#include "Coordinator.h"

namespace TDEngine {
namespace ECS {

    class CollisionSystem : public System {
    public:
        void Init(Coordinator* coordinator);

        // 毎フレームの衝突判定クエリを発行する
        void Update();

    private:
        Coordinator* m_coordinator = nullptr;
    };

} // namespace ECS
} // namespace TDEngine
