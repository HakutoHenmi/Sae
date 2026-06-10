#include "CollisionSystem.h"
#include "Components.h"
#include "Renderer.h"
#include "SpatialPartition.h"
#include <vector>
#include <set>

namespace TDEngine {
namespace ECS {

    void CollisionSystem::Init(Coordinator* coordinator) {
        m_coordinator = coordinator;
    }

    void CollisionSystem::Update() {
        if (!m_coordinator) return;

        auto* renderer = Engine::Renderer::GetInstance();
        if (!renderer) return;

        // 空間分割の範囲（ステージサイズに合わせて調整）
        Engine::Rect worldBounds = { -100.0f, -100.0f, 200.0f, 200.0f };
        Engine::QuadTree tree(worldBounds);

        // 1. 全エンティティをQuadTreeに登録
        for (auto const& entity : m_entities) {
            auto& transform = m_coordinator->GetComponent<TransformComponent>(entity);
            tree.Insert(entity, transform.translate);
        }

        // 2. 近接ペアのみを抽出してGPUにDispatch
        renderer->BeginCollisionCheck(1024); // 最大ペア数

        uint32_t pairIndex = 0;
        std::set<std::pair<Entity, Entity>> processedPairs;

        for (auto const& entityA : m_entities) {
            auto& transformA = m_coordinator->GetComponent<TransformComponent>(entityA);
            
            // クエリ範囲（AABBの大きさに合わせる）
            Engine::Rect queryRange = { 
                transformA.translate.x - 5.0f, 
                transformA.translate.z - 5.0f, 
                10.0f, 10.0f 
            };

            std::vector<Entity> nearbyEntities;
            tree.Query(queryRange, nearbyEntities);

            for (auto entityB : nearbyEntities) {
                if (entityA == entityB) continue;

                // 重複ペア(A,B)と(B,A)を避ける
                Entity first = (std::min)(entityA, entityB);
                Entity second = (std::max)(entityA, entityB);
                if (processedPairs.count({first, second})) continue;
                processedPairs.insert({first, second});

                auto& renderA = m_coordinator->GetComponent<RenderComponent>(entityA);
                auto& transformB = m_coordinator->GetComponent<TransformComponent>(entityB);
                auto& renderB = m_coordinator->GetComponent<RenderComponent>(entityB);

                Engine::Transform engTrA, engTrB;
                engTrA.scale = transformA.scale;
                engTrA.rotate = transformA.rotate;
                engTrA.translate = transformA.translate;
                
                engTrB.scale = transformB.scale;
                engTrB.rotate = transformB.rotate;
                engTrB.translate = transformB.translate;

                renderer->DispatchCollision(
                    renderA.meshHandle, engTrA,
                    renderB.meshHandle, engTrB,
                    pairIndex++
                );

                if (pairIndex >= 1024) break;
            }
            if (pairIndex >= 1024) break;
        }

        renderer->EndCollisionCheck();
    }

} // namespace ECS
} // namespace TDEngine
