#pragma pack_matrix(row_major)
struct Particle {
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 force;
    float pad1;
};

StructuredBuffer<Particle> Particles : register(t0);

cbuffer ViewProjection : register(b0) {
    matrix view;
    matrix projection;
    matrix viewProj;
    matrix invProjection;
    matrix invView;
    float3 cameraPos;
    float time;
    float3 corePosition;
    float isLiquidated;
    float3 blobColor;
    float padColor;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 viewPos : POSITION0;
    float3 worldPos : POSITION1;
    float3 centerWorldPos : POSITION2;
    float radius : BLENDWEIGHT0;
    float3 color : COLOR;
};

static const float2 quadOffsets[4] = {
    float2(-1.0f,  1.0f), float2( 1.0f,  1.0f),
    float2(-1.0f, -1.0f), float2( 1.0f, -1.0f)
};

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    VSOutput output;
    Particle p = Particles[instanceID];

    // ローカル座標 → ワールド座標（プレイヤー追従の核心）
    float3 centerWorld = p.position + corePosition;
    float radius = 0.18f; // 参考画像のような密集した小粒ドット表現

    float2 offset = quadOffsets[vertexID];
    float3 up = float3(view._12, view._22, view._32);
    float3 right = float3(view._11, view._21, view._31);
    float3 worldPos = centerWorld + right * offset.x * radius + up * offset.y * radius;

    float4 worldPos4 = float4(worldPos, 1.0f);
    float4 vPos = mul(worldPos4, view);
    output.pos = mul(worldPos4, viewProj);
    output.uv = offset;
    output.viewPos = vPos.xyz;
    output.worldPos = worldPos;
    output.centerWorldPos = centerWorld;
    output.radius = radius;

    // 速度ベースのヒートマップ（参考画像風: 速い=赤、中間=緑黄、遅い=青）
    float speedT = saturate(length(p.velocity) / 3.0f);
    float radialT = saturate(length(p.position) / 1.2f);
    float t = lerp(radialT, speedT, 0.5f); // 位置と速度を混合
    float3 heatColor = lerp(float3(0.1f, 0.4f, 1.0f), float3(0.2f, 0.9f, 0.3f), saturate(t * 2.0f));
    heatColor = lerp(heatColor, float3(1.0f, 0.2f, 0.1f), saturate((t - 0.5f) * 2.0f));
    output.color = heatColor;

    return output;
}
