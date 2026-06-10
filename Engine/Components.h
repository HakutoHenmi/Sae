#pragma once
#include "Matrix4x4.h"
#include <string>

namespace Game {
    class GimmickBase;
}

namespace TDEngine {
namespace ECS {

    // -------------------------------------------------------------
    // Transform Component
    // 位置・回転・スケールを保持する
    // -------------------------------------------------------------
    struct TransformComponent {
        Engine::Vector3 scale{1.0f, 1.0f, 1.0f};
        Engine::Vector3 rotate{0.0f, 0.0f, 0.0f};
        Engine::Vector3 translate{0.0f, 0.0f, 0.0f};

        Engine::Matrix4x4 ToMatrix() const {
#ifdef _WINDOWS
            using namespace DirectX;
            XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
            XMMATRIX R = XMMatrixRotationRollPitchYaw(rotate.x, rotate.y, rotate.z);
            XMMATRIX T = XMMatrixTranslation(translate.x, translate.y, translate.z);
            Engine::Matrix4x4 out;
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), S * R * T);
            return out;
#else
            // 非Windows環境用ダミー実装
            return Engine::Matrix4x4{};
#endif
        }
    };

    // -------------------------------------------------------------
    // Render Component
    // 描画に必要なハンドル等の情報を保持する
    // -------------------------------------------------------------
    struct RenderComponent {
        uint32_t meshHandle = 0;
        uint32_t textureHandle = 0;
        Engine::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        bool isVisible = true;

        std::string textureName = "";
        std::string shaderName = "Default";
        std::string modelFileName = "";
    };

    // -------------------------------------------------------------
    // Collision Component
    // 衝突判定用（AABB、メッシュコライダ情報）を保持する
    // -------------------------------------------------------------
    struct CollisionComponent {
        Engine::Vector3 localAABBMin = {-1.0f, -1.0f, -1.0f};
        Engine::Vector3 localAABBMax = {1.0f, 1.0f, 1.0f};
        const void* collisionMesh = nullptr;
        bool useMeshCollision = true;
    };

    // -------------------------------------------------------------
    // Info/Meta Component
    // エディタ用やタグ等、エンティティのメタ情報を保持する
    // -------------------------------------------------------------
    struct InfoComponent {
        std::string name = "Entity";
        uint32_t type = 0;
        bool isLocked = false;
    };

    // -------------------------------------------------------------
    // Gimmick Component
    // ロジック（Game層のクラス）紐づけ用情報を保持する
    // -------------------------------------------------------------
    struct GimmickComponent {
        Game::GimmickBase* gimmick = nullptr;
        std::string gimmickName = "";
    };

} // namespace ECS
} // namespace TDEngine
