// Engine/RibbonRenderer.cpp
#include "RibbonRenderer.h"
#include <algorithm>

namespace Engine {

void RibbonRenderer::Initialize(Renderer* renderer, uint32_t maxSegments) {
    renderer_ = renderer;
    maxSegments_ = maxSegments;
    
    std::vector<VertexData> v(maxSegments_ * 2);
    std::vector<uint32_t> idx((maxSegments_ - 1) * 6);
    for(uint32_t i=0; i < maxSegments_ - 1; ++i) {
        idx[i*6+0] = i*2+0; idx[i*6+1] = i*2+1; idx[i*6+2] = (i+1)*2+0;
        idx[i*6+3] = (i+1)*2+0; idx[i*6+4] = i*2+1; idx[i*6+5] = (i+1)*2+1;
    }
    mesh_ = renderer_->CreateDynamicMesh(v, idx);
}

void RibbonRenderer::Update(float dt) {
    for (auto& p : points_) {
        p.life -= dt;
    }
    while (!points_.empty() && points_.front().life <= 0) {
        points_.pop_front();
    }
}

void RibbonRenderer::AddPoint(const Vector3& top, const Vector3& bottom, float life) {
    points_.push_back({top, bottom, life, life});
    if (points_.size() > maxSegments_) {
        points_.pop_front();
    }
}

void RibbonRenderer::Draw(uint32_t texture, const Vector4& color, const std::string& shaderName) {
    if (points_.size() < 2) return;

    std::vector<VertexData> vertices;
    vertices.reserve(points_.size() * 2);

    for (size_t i = 0; i < points_.size(); ++i) {
        float u = (float)i / (points_.size() - 1);
        float alpha = points_[i].life / points_[i].maxLife;
        Vector4 finalCol = {color.x, color.y, color.z, color.w * alpha};

        VertexData vTop{};
        vTop.position = {points_[i].top.x, points_[i].top.y, points_[i].top.z, 1.0f};
        vTop.texcoord = {u, 0.0f};
        vTop.normal = {0, 0, 1}; // Dummy
        
        VertexData vBottom{};
        vBottom.position = {points_[i].bottom.x, points_[i].bottom.y, points_[i].bottom.z, 1.0f};
        vBottom.texcoord = {u, 1.0f};
        vBottom.normal = {0, 0, 1};

        vertices.push_back(vTop);
        vertices.push_back(vBottom);
    }

    renderer_->UpdateDynamicMesh(mesh_, vertices);
    
    // Draw with no world transform (points are in world space)
    renderer_->DrawMesh(mesh_, texture, Matrix4x4::Identity(), color, shaderName);
}

void RibbonRenderer::Clear() {
    points_.clear();
}

} // namespace Engine
