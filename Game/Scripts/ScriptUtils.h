#pragma once
#include "../../externals/entt/entt.hpp"
#include "../ObjectTypes.h"
#include "Scenes/MainScene.h"
#include <memory>
#include <vector>

namespace Game {

/**
 * @brief スクリプト関連の便利関数を提供するユーティリティクラス
 */
class ScriptUtils {
public:
    /**
     * @brief 指定したエンティティから特定の種類のスクリプトを取得します。
     * @tparam T 取得したいスクリプトのクラス型
     * @param entity 対象のエンティティ
     * @param scene 現在のシーン
     * @return std::shared_ptr<T> スクリプトのポインタ。見つからない場合は nullptr。
     */
    template <typename T>
    static std::shared_ptr<T> GetScript(entt::entity entity, MainScene* scene) {
        if (!scene) return nullptr;
        
        auto& registry = scene->GetRegistry();
        if (registry.all_of<ScriptComponent>(entity)) {
            auto& sc = registry.get<ScriptComponent>(entity);
            for (auto& entry : sc.scripts) {
                if (auto script = std::dynamic_pointer_cast<T>(entry.instance)) {
                    return script;
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief シーン内から最初に見つかった特定の種類のスクリプトを取得します。
     * @tparam T 取得したいスクリプトのクラス型
     * @param scene 現在のシーン
     * @return std::shared_ptr<T> スクリプトのポインタ。見つからない場合は nullptr。
     */
    template <typename T>
    static std::shared_ptr<T> FindScript(MainScene* scene) {
        if (!scene) return nullptr;

        auto& registry = scene->GetRegistry();
        auto view = registry.view<ScriptComponent>();
        for (auto entity : view) {
            if (auto script = GetScript<T>(entity, scene)) {
                return script;
            }
        }
        return nullptr;
    }

    /**
     * @brief シーン内から特定の種類のスクリプトをすべて取得します。
     * @tparam T 取得したいスクリプトのクラス型
     * @param scene 現在のシーン
     * @return std::vector<std::shared_ptr<T>> スクリプトのポインタ配列
     */
    template <typename T>
    static std::vector<std::shared_ptr<T>> FindScripts(MainScene* scene) {
        std::vector<std::shared_ptr<T>> results;
        if (!scene) return results;

        auto& registry = scene->GetRegistry();
        auto view = registry.view<ScriptComponent>();
        for (auto entity : view) {
            auto& sc = registry.get<ScriptComponent>(entity);
            for (auto& entry : sc.scripts) {
                if (auto script = std::dynamic_pointer_cast<T>(entry.instance)) {
                    results.push_back(script);
                }
            }
        }
        return results;
    }
};

} // namespace Game
