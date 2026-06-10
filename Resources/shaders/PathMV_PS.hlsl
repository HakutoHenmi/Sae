#include "Obj.hlsli"

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);
    float3 base = color.rgb;

    // 上面判定を強める（0〜1）
    float up = saturate(n.y);
    up = smoothstep(0.2, 0.9, up); // ←ここが効く

    // 上面：明るく・少し青寄り
    float3 topColor = base * float3(1.15, 1.15, 1.25);

    // 側面：暗く・少し紫寄り
    float3 sideColor = base * float3(0.75, 0.80, 0.95);

    float3 faceColor = lerp(sideColor, topColor, up);

    // 高さグラデーション（弱めに）
    float h = saturate((i.worldpos.y + 2.0) * 0.20);
    float3 grad = lerp(float3(0.95, 0.95, 1.00), float3(1.05, 1.05, 1.10), h);

    return float4(faceColor * grad, 1.0);
}
