#include "IScript.h"
#include "Scenes/MainScene.h"
#include "../Engine/EventSystem.h"
#include "ObjectTypes.h"

namespace Game {

void IScript::Emit(MainScene* scene, const std::string& eventName, float value) {
    if (scene) scene->GetEventSystem().Emit(eventName, value);
}

void IScript::EmitString(MainScene* scene, const std::string& eventName, const std::string& value) {
    if (scene) scene->GetEventSystem().EmitString(eventName, value);
}

void IScript::EmitVoid(MainScene* scene, const std::string& eventName) {
    if (scene) scene->GetEventSystem().EmitVoid(eventName);
}

void IScript::Subscribe(MainScene* scene, const std::string& eventName, std::function<void(float)> callback) {
    if (scene) scene->GetEventSystem().Subscribe(eventName, callback);
}

void IScript::SubscribeString(MainScene* scene, const std::string& eventName, std::function<void(const std::string&)> callback) {
    if (scene) scene->GetEventSystem().SubscribeString(eventName, callback);
}

void IScript::SubscribeVoid(MainScene* scene, const std::string& eventName, std::function<void()> callback) {
    if (scene) scene->GetEventSystem().SubscribeVoid(eventName, callback);
}

void IScript::SetVar(entt::entity entity, MainScene* scene, const std::string& key, float value) {
    if (!scene) return;
    auto& registry = scene->GetRegistry();
    if (!registry.all_of<VariableComponent>(entity)) {
        registry.emplace<VariableComponent>(entity);
    }
    registry.get<VariableComponent>(entity).SetValue(key, value);
}

void IScript::SetVarString(entt::entity entity, MainScene* scene, const std::string& key, const std::string& value) {
    if (!scene) return;
    auto& registry = scene->GetRegistry();
    if (!registry.all_of<VariableComponent>(entity)) {
        registry.emplace<VariableComponent>(entity);
    }
    registry.get<VariableComponent>(entity).SetString(key, value);
}

float IScript::GetVar(entt::entity entity, MainScene* scene, const std::string& key, float defaultVal) {
    if (!scene) return defaultVal;
    auto& registry = scene->GetRegistry();
    if (registry.all_of<VariableComponent>(entity)) {
        return registry.get<VariableComponent>(entity).GetValue(key, defaultVal);
    }
    return defaultVal;
}

std::string IScript::GetVarString(entt::entity entity, MainScene* scene, const std::string& key, const std::string& defaultVal) {
    if (!scene) return defaultVal;
    auto& registry = scene->GetRegistry();
    if (registry.all_of<VariableComponent>(entity)) {
        return registry.get<VariableComponent>(entity).GetString(key, defaultVal);
    }
    return defaultVal;
}

} // namespace Game
