#ifndef GPUPARTICLE_H
#define GPUPARTICLE_H

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "Matrix4x4.h"

namespace Engine {

struct GPUParticleData {
    DirectX::XMFLOAT3 position;
    float life;
    DirectX::XMFLOAT3 velocity;
    float age;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT3 scale;
    float pad;
};

struct GPUParticleEmitterData {
    DirectX::XMFLOAT3 emitPos;
    float emitRate;
    DirectX::XMFLOAT3 emitVel;
    float emitLife;
};

class GPUParticleSystem {
public:
    GPUParticleSystem() = default;
    ~GPUParticleSystem();

    bool Initialize(ID3D12Device* device, uint32_t maxParticles);
    void Update(ID3D12GraphicsCommandList* cmd, float dt, const GPUParticleEmitterData& emitterData);
    void Draw(ID3D12GraphicsCommandList* cmd, const Matrix4x4& viewProj, const DirectX::XMFLOAT3& camPos, bool useBillboard = true);

private:
    bool CreateBuffers(ID3D12Device* device);
    bool CreatePipelines(ID3D12Device* device);

private:
    uint32_t maxParticles_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePSO_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> drawPSO_;
    
    bool isInitialized_ = false;
    bool initDone_ = false;
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
};

} // namespace Engine
#endif // GPUPARTICLE_H
