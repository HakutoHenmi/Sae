// ========================================================================
// SPH流体シミュレーション v3 — パラメータバランス完全再計算版
// 
// 設計根拠:
//   6144粒子を半径1.0の球に詰めた場合:
//     平均粒子間隔 ≈ (4/3 π / 6144)^(1/3) ≈ 0.088
//     smoothingLength = 3.5 × 間隔 ≈ 0.30（近傍約30粒子）
//     particleMass = 0.1 → 総質量 614.4
//     restDensity ≈ 総質量 / 体積 ≈ 147 → 150に設定
//
//   圧力: Tait式（指数4、7より柔らかくプルプルする）
//   表面張力: 粒同士の引力で自然な丸みを保つ
//   境界拘束: なし（SPH自体の表面張力と圧力で形を保つ）
// ========================================================================

struct Particle {
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 force;
    float pad1;
};

cbuffer FluidConstants : register(b0) {
    float3 corePosition;
    uint numParticles;
    
    float deltaTime;
    float smoothingLength;
    float particleMass;
    float restDensity;
    
    float gasStiffness;
    float viscosity;
    float gravity;
    float damping;
    
    float floorWorldY;
    uint passType;
    uint simMode;
    float pad0;
    float3 blobRadii;
    float pad1;
};

RWStructuredBuffer<Particle> Particles : register(u0);

#define PI 3.14159265359f

// ---- SPHカーネル関数 ----
float Poly6(float r2, float h) {
    float h2 = h * h;
    if (r2 < 0.0001f || r2 > h2) return 0.0f;
    float t = h2 - r2;
    return (315.0f / (64.0f * PI * pow(h, 9))) * t * t * t;
}

float3 SpikyGrad(float3 dir, float r, float h) {
    if (r < 0.0001f || r > h) return float3(0, 0, 0);
    float t = h - r;
    return dir * (-45.0f / (PI * pow(h, 6))) * t * t;
}

float ViscosityLap(float r, float h) {
    if (r < 0.0001f || r > h) return 0.0f;
    return (45.0f / (PI * pow(h, 6))) * (h - r);
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint id = DTid.x;
    if (id >= numParticles) return;

    Particle p = Particles[id];
    float h = smoothingLength;

    // ==== パス0: 密度計算 + Tait圧力 ====
    if (passType == 0) {
        float density = 0.0f;
        for (uint i = 0; i < numParticles; i++) {
            float3 diff = p.position - Particles[i].position;
            density += particleMass * Poly6(dot(diff, diff), h);
        }
        density = max(density, 0.001f);
        p.density = density;
        
        // Tait式（指数4: 7より柔らかくゼリーのように変形しやすい）
        float ratio = density / restDensity;
        p.pressure = gasStiffness * (ratio * ratio * ratio * ratio - 1.0f);
        
        Particles[id] = p;
    }
    // ==== パス1: 力の計算 + 位置更新 ====
    else if (passType == 1) {
        float3 pressureForce = float3(0, 0, 0);
        float3 viscosityForce = float3(0, 0, 0);
        float3 surfaceTensionForce = float3(0, 0, 0);
        
        // 表面張力係数（スライム時は非常に強く → ドーム型を保つ、液状化時は弱く → 広がる）
        float cohesion = simMode == 0 ? 0.6f : 0.01f;
        
        for (uint i = 0; i < numParticles; i++) {
            if (i == id) continue;
            Particle neighbor = Particles[i];
            float3 diff = p.position - neighbor.position;
            float r = length(diff);
            
            if (r > 0.0001f && r < h) {
                float3 dir = diff / r;
                
                // 圧力（対称形式: 運動量保存を保証）
                float pTerm = (p.pressure / max(p.density * p.density, 0.01f))
                            + (neighbor.pressure / max(neighbor.density * neighbor.density, 0.01f));
                pressureForce -= particleMass * pTerm * SpikyGrad(dir, r, h);
                
                // 粘性（スライム時は高粘度でゼリーのように、液状化時は低粘度で水のように）
                float3 velDiff = neighbor.velocity - p.velocity;
                viscosityForce += velDiff * (particleMass / max(neighbor.density, 0.01f)) * ViscosityLap(r, h);
                
                // 表面張力（Poly6ベースの引力: 粒同士が引き合って丸みを保つ）
                float w = Poly6(r * r, h);
                surfaceTensionForce -= dir * cohesion * particleMass * w;
            }
        }
        viscosityForce *= viscosity;
        
        // 中心への引き戻し力（モードに応じて強さを変える）
        float3 coreForce = float3(0, 0, 0);
        float distToCore = length(p.position);
        if (distToCore > 0.1f) {
            float3 dirToCore = -p.position / distToCore;
            if (simMode == 0) {
                // スライム: 距離の2乗に比例（遠い粒ほど強烈に引き戻す → 取り残しゼロ）
                float pullStrength = 8.0f;
                coreForce = dirToCore * pullStrength * distToCore * distToCore;
            } else {
                // 液状化: 弱い引き戻し（完全に散らばらず、ある程度まとまる）
                float pullStrength = 0.3f;
                coreForce = dirToCore * pullStrength * distToCore;
            }
        }
        
        // 重力
        float3 gravityForce = float3(0, gravity * p.density, 0);
        
        // 力の合成
        float3 totalForce = pressureForce + viscosityForce + surfaceTensionForce + coreForce * p.density + gravityForce;
        float3 accel = totalForce / max(p.density, 0.01f);
        
        // 速度・位置更新
        p.velocity += accel * deltaTime;
        p.velocity *= damping;
        
        // 安定性のための速度制限
        float maxSpeed = simMode == 0 ? 8.0f : 6.0f;
        float speed = length(p.velocity);
        if (speed > maxSpeed) {
            p.velocity *= maxSpeed / speed;
        }
        
        p.position += p.velocity * deltaTime;
        
        // スライムモード: 遠すぎる粒を強制回収（オーバーシュート防止）
        if (simMode == 0) {
            float dist = length(p.position);
            float maxRadius = 1.2f; // この半径を超えた粒は強制的に戻す
            if (dist > maxRadius) {
                p.position = p.position * (maxRadius / dist);
                p.velocity *= 0.3f; // 速度を大幅に減衰させてバウンド防止
            }
        }
        
        // 地面衝突
        float colRadius = 0.08f;
        float worldY = corePosition.y + p.position.y;
        if (worldY < floorWorldY + colRadius) {
            p.position.y = floorWorldY + colRadius - corePosition.y;
            p.velocity.y *= -0.1f;  // 跳ね返りを抑える
            float friction = simMode == 0 ? 0.7f : 0.95f;
            p.velocity.x *= friction;
            p.velocity.z *= friction;
        }
        
        Particles[id] = p;
    }
}
