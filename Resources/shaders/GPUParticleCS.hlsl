// GPUParticle Compute Shader
// ルートディスクリプタ方式: u0 のみ使用

struct Particle {
    float3 position;
    float life;
    float3 velocity;
    float age;
    float4 color;
    float3 scale;
    float pad;
};

cbuffer cbSystem : register(b0) {
    float dt;
    float totalTime;
    uint maxParticles;
    uint emitCountThisFrame;
    float3 emitPos;
    float emitRate;
    float3 emitVel;
    float emitLife;
};

RWStructuredBuffer<Particle> g_ParticlePool : register(u0);

// 簡易ハッシュ関数 (疑似乱数生成用)
float hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return float(seed) / 4294967295.0;
}

// 03_01: 初期化 - 全パーティクルを死亡状態にする
[numthreads(256, 1, 1)]
void InitCS(uint3 dtid : SV_DispatchThreadID) {
    if(dtid.x >= maxParticles) return;
    g_ParticlePool[dtid.x].life = -1.0;
    g_ParticlePool[dtid.x].age = 0.0;
    g_ParticlePool[dtid.x].position = float3(0, 0, 0);
    g_ParticlePool[dtid.x].velocity = float3(0, 0, 0);
    g_ParticlePool[dtid.x].color = float4(0, 0, 0, 0);
    g_ParticlePool[dtid.x].scale = float3(0.3, 0.3, 0.3);
}

// 03_02 & 03_04: パーティクルの発生と使い回し
[numthreads(64, 1, 1)]
void EmitCS(uint3 dtid : SV_DispatchThreadID) {
    if(dtid.x >= emitCountThisFrame) return;
    
    // 死亡中のスロットを探す (ハッシュベースのスキャン)
    uint seed = dtid.x + asuint(totalTime);
    uint startIdx = uint(hash(seed) * float(maxParticles));
    
    for(uint i = 0; i < maxParticles; ++i) {
        uint idx = (startIdx + i) % maxParticles;
        
        // life < 0 なら死亡中 → 再利用
        if(g_ParticlePool[idx].life < 0.0) {
            Particle p;
            float r1 = hash(seed + i * 3u + 1u) * 2.0 - 1.0;
            float r2 = hash(seed + i * 7u + 2u) * 2.0 - 1.0;
            float r3 = hash(seed + i * 13u + 3u) * 2.0 - 1.0;
            
            p.position = emitPos + float3(r1, r2, r3) * 0.5;
            p.velocity = emitVel + float3(r1 * 0.5, abs(r2) * 2.0, r3 * 0.5);
            p.life = emitLife + hash(seed + i * 17u) * 0.5;
            p.age = 0.0;
            p.color = float4(1.0, 0.8, 0.3, 1.0); // 暖色のパーティクル
            p.scale = float3(0.3, 0.3, 0.3);
            p.pad = 0;
            
            g_ParticlePool[idx] = p;
            return; // 1スレッドにつき1パーティクル
        }
    }
}

// 03_03: パーティクルの更新
[numthreads(256, 1, 1)]
void UpdateCS(uint3 dtid : SV_DispatchThreadID) {
    if(dtid.x >= maxParticles) return;
    
    Particle p = g_ParticlePool[dtid.x];
    if(p.life <= 0.0) return; // 死亡中はスキップ
    
    // 更新
    p.age += dt;
    p.velocity.y -= 1.0 * dt; // 重力
    p.position += p.velocity * dt;
    
    // 寿命チェック
    float t = p.age / p.life;
    if(t >= 1.0) {
        p.life = -1.0; // 死亡 → 03_04: 再利用可能に
    } else {
        // 色のフェード (暖色 → 透明)
        p.color.a = 1.0 - t;
        p.color.rgb = lerp(float3(1.0, 0.8, 0.3), float3(1.0, 0.2, 0.1), t);
        // サイズの縮小
        float s = lerp(0.3, 0.05, t);
        p.scale = float3(s, s, s);
    }
    
    g_ParticlePool[dtid.x] = p;
}
