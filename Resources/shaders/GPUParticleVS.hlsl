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
    row_major float4x4 viewProj;
    float3 camPos;
    uint useBillboard;
};

StructuredBuffer<Particle> g_ParticlePool : register(t0);

struct VSOut {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

VSOut main(uint vId : SV_VertexID, uint instId : SV_InstanceID) {
    VSOut o;
    
    Particle p = g_ParticlePool[instId];
    
    // 簡単なビルボードの頂点計算(vId 0~5 でQuadを生成)
    float2 uvs[6] = { float2(0,0), float2(1,0), float2(0,1), float2(0,1), float2(1,0), float2(1,1) };
    float2 offsets[6] = { float2(-1,1), float2(1,1), float2(-1,-1), float2(-1,-1), float2(1,1), float2(1,-1) };
    
    float3 right = float3(1, 0, 0);
    float3 up = float3(0, 1, 0);
    
    if (useBillboard > 0) {
        // カメラに向けるビルボード計算
        float3 look = normalize(camPos - p.position);
        float3 worldUp = float3(0, 1, 0);
        
        // 真上や真下を向いている場合の対策
        if (abs(look.y) > 0.999) {
            worldUp = float3(0, 0, 1);
        }
        
        right = normalize(cross(worldUp, look));
        up = cross(look, right);
    }
    
    float3 worldPos = p.position + right * offsets[vId].x * p.scale.x + up * offsets[vId].y * p.scale.y;
    
    o.pos = mul(float4(worldPos, 1.0), viewProj);
    o.color = p.color;
    o.uv = uvs[vId];
    
    return o;
}
