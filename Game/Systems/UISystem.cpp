#include "UISystem.h"
#include "../ObjectTypes.h"
#include "../../Engine/Renderer.h"
#include "../../Engine/Input.h"
#include "../../Engine/SceneManager.h"
#include "../Scripts/IScript.h"
#include "../../Engine/WindowDX.h"
#include "../../externals/imgui/imgui.h"
#include <unordered_map>
#include <set>
#include <algorithm>

namespace Game {

void UISystem::Update(entt::registry& /*registry*/, GameContext& /*ctx*/) {
}

UISystem::WorldRect UISystem::CalculateWorldRect(entt::entity entity, entt::registry& registry, float screenW, float screenH) {
    if (!registry.all_of<RectTransformComponent>(entity)) return {0, 0, 0, 0};

    std::vector<entt::entity> path;
    entt::entity current = entity;
    while (registry.valid(current)) {
        path.push_back(current);
        entt::entity parent = entt::null;
        
        if (registry.all_of<HierarchyComponent>(current)) {
            entt::entity parentId = registry.get<HierarchyComponent>(current).parentId;
            if (parentId != entt::null) {
                parent = parentId;
            }
        }
        current = parent;
    }
    std::reverse(path.begin(), path.end());

    WorldRect currentRect = { 0, 0, screenW, screenH };

    for (entt::entity pObj : path) {
        if (!registry.all_of<RectTransformComponent>(pObj)) continue;
        auto& rect = registry.get<RectTransformComponent>(pObj);
        
        float worldW = rect.size.x;
        float worldH = rect.size.y;
        float anchorX = currentRect.x + currentRect.w * rect.anchor.x;
        float anchorY = currentRect.y + currentRect.h * rect.anchor.y;
        float worldX = anchorX - worldW * rect.pivot.x + rect.pos.x;
        float worldY = anchorY - worldH * rect.pivot.y + rect.pos.y;
        
        currentRect = { worldX, worldY, worldW, worldH };
    }
    return currentRect;
}

void UISystem::Draw(entt::registry& registry, GameContext& ctx) {
    std::unordered_map<uint32_t, WorldRect> cache;

    auto renderRecursive = [&](auto self, entt::entity parentId, WorldRect parentRect) -> void {
        auto view = registry.view<RectTransformComponent>();
        for (auto e : view) {
            entt::entity currentParentId = entt::null;
            if (registry.all_of<HierarchyComponent>(e)) {
                currentParentId = registry.get<HierarchyComponent>(e).parentId;
            }

            if (currentParentId == parentId) {
                auto& rect = view.get<RectTransformComponent>(e);
                if (!rect.enabled) continue;
                float worldW = rect.size.x;
                float worldH = rect.size.y;
                float anchorX = parentRect.x + parentRect.w * rect.anchor.x;
                float anchorY = parentRect.y + parentRect.h * rect.anchor.y;
                float worldX = anchorX - worldW * rect.pivot.x + rect.pos.x;
                float worldY = anchorY - worldH * rect.pivot.y + rect.pos.y;
                
                WorldRect selfRect = { worldX, worldY, worldW, worldH };
                uint32_t eId = static_cast<uint32_t>(e);
                cache[eId] = selfRect;

                RenderNodeWithRect(e, registry, selfRect, ctx);
                self(self, e, selfRect);
            }
        }
    };

	float vw = ctx.viewportSize.x > 0 ? ctx.viewportSize.x : (float)Engine::WindowDX::kW;
	float vh = ctx.viewportSize.y > 0 ? ctx.viewportSize.y : (float)Engine::WindowDX::kH;
	WorldRect screen = { 0.0f, 0.0f, vw, vh };
	renderRecursive(renderRecursive, entt::null, screen);

	auto viewRawText = registry.view<TransformComponent, UITextComponent>();
	viewRawText.each([&](entt::entity e, TransformComponent& transform, UITextComponent& text) {
		if (registry.all_of<RectTransformComponent>(e)) return; 
		if (text.enabled) {
			DrawTextW(e, registry, text, transform.translate.x, transform.translate.y, 0.0f, 0.0f, ctx.renderer);
		}
	});
}

void UISystem::DrawUI(entt::registry& /*registry*/, GameContext& ctx) {
    if (!ctx.camera) return;

#ifdef USE_IMGUI
    ImDrawList* drawList = ImGui::GetForegroundDrawList(); 
    if (!drawList) return;
    if (!ctx.scene) return;
#endif
}

bool UISystem::WorldToScreen(const DirectX::XMFLOAT3& worldPos, const Engine::Camera& camera, float& screenX, float& screenY) {
    return WorldToScreenWithView(worldPos, camera, {0, 0}, {(float)Engine::WindowDX::kW, (float)Engine::WindowDX::kH}, screenX, screenY);
}

bool UISystem::WorldToScreenWithView(const DirectX::XMFLOAT3& worldPos, const Engine::Camera& camera, const DirectX::XMFLOAT2& viewOffset, const DirectX::XMFLOAT2& viewSize, float& screenX, float& screenY) {
    DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&worldPos);
    
    DirectX::XMMATRIX view = camera.View();
    DirectX::XMMATRIX proj = camera.Proj();
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

    float vw = std::max(1.0f, viewSize.x);
    float vh = std::max(1.0f, viewSize.y);
    DirectX::XMVECTOR screenPos = DirectX::XMVector3Project(p, 0, 0, vw, vh, 0.0f, 1.0f, proj, view, world);
    
    DirectX::XMFLOAT3 sp;
    DirectX::XMStoreFloat3(&sp, screenPos);

    DirectX::XMMATRIX vp = view * proj;
    DirectX::XMVECTOR clipPos = DirectX::XMVector3TransformCoord(p, vp);
    float cz = DirectX::XMVectorGetZ(clipPos);
    if (cz < 0.0f || cz > 1.0f) return false;

    screenX = viewOffset.x + sp.x;
    screenY = viewOffset.y + sp.y;
    
    return true;
}

void UISystem::Reset(entt::registry& /*registry*/) {
    deathTimer_ = 0.0f;
}

void UISystem::RenderNodeWithRect(entt::entity entity, entt::registry& registry, const WorldRect& wr, GameContext& ctx) {
    if (registry.all_of<UIButtonComponent>(entity)) {
        auto& btn = registry.get<UIButtonComponent>(entity);
        ProcessButton(entity, registry, btn, wr.x, wr.y, wr.w, wr.h, ctx);
    }

    DirectX::XMFLOAT4 buttonColor = { 1, 1, 1, 1 };
    if (registry.all_of<UIButtonComponent>(entity)) {
        auto& btn = registry.get<UIButtonComponent>(entity);
        if (btn.isPressed) buttonColor = btn.pressedColor;
        else if (btn.isHovered) buttonColor = btn.hoverColor;
        else buttonColor = btn.normalColor;
    }

    if (registry.all_of<UIImageComponent>(entity)) {
        auto& img = registry.get<UIImageComponent>(entity);
        if (img.enabled) {
            if (registry.all_of<UIButtonComponent>(entity)) {
                Engine::Renderer::SpriteDesc border;
                border.x = wr.x - 2.0f;
                border.y = wr.y - 2.0f;
                border.w = wr.w + 4.0f;
                border.h = wr.h + 4.0f;
                border.color = { 1.0f, 1.0f, 1.0f, 1.0f };
                border.layer = img.layer;
                ctx.renderer->DrawSprite(0, border);
            }

            DirectX::XMFLOAT4 finalColor = { img.color.x * buttonColor.x, img.color.y * buttonColor.y, img.color.z * buttonColor.z, img.color.w * buttonColor.w };
            if (img.is9Slice) {
                Engine::Renderer::Sprite9SliceDesc s;
                s.x = wr.x; s.y = wr.y; s.w = wr.w; s.h = wr.h;
                s.left = img.borderLeft; s.right = img.borderRight; s.top = img.borderTop; s.bottom = img.borderBottom;
                s.color = { finalColor.x, finalColor.y, finalColor.z, finalColor.w };
                s.rotationRad = DirectX::XMConvertToRadians(registry.get<RectTransformComponent>(entity).rotation);
                s.layer = img.layer;
                ctx.renderer->DrawSprite9Slice(img.textureHandle, s);
            } else {
                Engine::Renderer::SpriteDesc s;
                s.x = wr.x; s.y = wr.y; s.w = wr.w; s.h = wr.h;
                s.color = { finalColor.x, finalColor.y, finalColor.z, finalColor.w };
                s.rotationRad = DirectX::XMConvertToRadians(registry.get<RectTransformComponent>(entity).rotation);
                s.layer = img.layer;
                ctx.renderer->DrawSprite(img.textureHandle, s);
            }
        }
    }

    if (registry.all_of<UITextComponent>(entity)) {
        auto& text = registry.get<UITextComponent>(entity);
        if (text.enabled) {
            DrawTextW(entity, registry, text, wr.x, wr.y, wr.w, wr.h, ctx.renderer);
        }
    }
}

void UISystem::DrawTextW(entt::entity /*entity*/, entt::registry& /*registry*/, const UITextComponent& text, float worldX, float worldY, float worldW, float worldH, Engine::Renderer* renderer) {
	if (!renderer || text.text.empty() || text.color.w <= 0.01f) return;

	float fontScale = text.fontSize / 64.0f;

	float tw = renderer->MeasureTextWidth(text.text, fontScale, text.fontPath);
	float th = renderer->GetTextLineHeight(fontScale, text.fontPath);

	float px = worldX;
	float py = worldY;
	if (worldW > 0.0f) px += (worldW - tw) * 0.5f;
	if (worldH > 0.0f) py += (worldH - th) * 0.5f;

	Engine::Vector4 shadowColor = { 0.0f, 0.0f, 0.0f, text.color.w };
	renderer->DrawString(text.text, px + 2.0f, py + 2.0f, fontScale, shadowColor, text.fontPath);

	Engine::Vector4 colorVec = { text.color.x, text.color.y, text.color.z, text.color.w };
	renderer->DrawString(text.text, px, py, fontScale, colorVec, text.fontPath);
}

void UISystem::ProcessButton(entt::entity entity, entt::registry& registry, UIButtonComponent& btn, float worldX, float worldY, float worldW, float worldH, GameContext& ctx) {
    if (!ctx.input) return;

    float mx, my;
    if (ctx.useOverrideMouse) {
        mx = ctx.overrideMouseX;
        my = ctx.overrideMouseY;
    } else {
        float fmx, fmy;
        ctx.input->GetMousePos(fmx, fmy);
        
        float rx = fmx - ctx.viewportOffset.x;
        float ry = fmy - ctx.viewportOffset.y;
        
        if (ctx.viewportSize.x > 0 && ctx.viewportSize.y > 0) {
            mx = rx * (float)Engine::WindowDX::kW / ctx.viewportSize.x;
            my = ry * (float)Engine::WindowDX::kH / ctx.viewportSize.y;
        } else {
            mx = rx;
            my = ry;
        }
    }

    float hw = worldW * btn.hitboxScale.x;
    float hh = worldH * btn.hitboxScale.y;
    float cx = worldX + worldW * 0.5f + btn.hitboxOffset.x;
    float cy = worldY + worldH * 0.5f + btn.hitboxOffset.y;
    float hx = cx - hw * 0.5f;
    float hy = cy - hh * 0.5f;

    bool hovered = (mx >= hx && mx <= hx + hw &&
                    my >= hy && my <= hy + hh);

    btn.isHovered = hovered;
    btn.isPressed = hovered && ctx.input->IsMouseDown(0); 

    if (hovered && ctx.input->IsMouseTrigger(0)) {
        if (registry.all_of<ScriptComponent>(entity)) {
            auto& sc = registry.get<ScriptComponent>(entity);
            if (sc.enabled) {
                for (auto& entry : sc.scripts) {
                    if (entry.instance) {
                    }
                }
            }
        }
    }
}

} // namespace Game
