// Engine/RibbonRenderer.h
#pragma once
#include <vector>
#include <deque>
#include "Renderer.h"
#include "Matrix4x4.h"

namespace Engine {

class RibbonRenderer {
public:
    struct Point {
        ::Engine::Vector3 top;
        ::Engine::Vector3 bottom;
        float life;
        float maxLife;
    };

    void Initialize(Renderer* renderer, uint32_t maxSegments = 100);
    void Update(float dt);
    void AddPoint(const ::Engine::Vector3& top, const ::Engine::Vector3& bottom, float life = 0.5f);
    void Draw(uint32_t texture, const ::Engine::Vector4& color = {1,1,1,1}, const std::string& shaderName = "ParticleAdditive");
    void Clear();

private:
    Renderer* renderer_ = nullptr;
    std::deque<Point> points_;
    uint32_t maxSegments_ = 100;
    uint32_t mesh_ = 0;
};

} // namespace Engine
