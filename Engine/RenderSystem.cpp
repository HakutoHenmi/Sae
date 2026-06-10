#include "RenderSystem.h"
#include "Components.h"
#include "Renderer.h"

namespace TDEngine {
namespace ECS {

    void RenderSystem::Init(Coordinator* coordinator) {
        m_coordinator = coordinator;
    }

    void RenderSystem::Draw() {
        if (!m_coordinator) return;

        // 各エンティティについて、TransformComponentとRenderComponentを取得し、
        // 既存のRendererに描画命令を投げる
        for (auto const& entity : m_entities) {
            auto& transform = m_coordinator->GetComponent<TransformComponent>(entity);
            auto& render = m_coordinator->GetComponent<RenderComponent>(entity);

            if (render.isVisible) {
                // Engine::Transformに変換 (既存システムとのブリッジ)
                Engine::Transform engTransform;
                engTransform.scale = transform.scale;
                engTransform.rotate = transform.rotate;
                engTransform.translate = transform.translate;

                // RendererのDrawMeshInstancedを呼び出し
                auto* renderer = Engine::Renderer::GetInstance();
                if (renderer) {
                    renderer->DrawMeshInstanced(
                        render.meshHandle,
                        render.textureHandle,
                        engTransform,
                        render.color,
                        render.shaderName
                    );
                }
            }
        }
    }

} // namespace ECS
} // namespace TDEngine
