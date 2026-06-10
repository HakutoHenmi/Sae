// StylizedGrassPS.hlsl
#include "Grass.hlsli"

// Light structures (Same as EnhancedTerrain)
struct DirLight { float3 dir; float pad0; float3 color; float pad1; uint enabled; float3 pad2; };
cbuffer CBLight : register(b2) {
    float3 gAmbientColor;
    float padA0;
    DirLight gDir[1];
    // 他のライトは省略（必要に応じて追加）
};

float4 main(VSOutput input) : SV_TARGET {
    // 1. Gradation Color
    // 根本の色(暗め)から先端の色(明るめ)へ
    // input.uv.y を使用 (0:先端, 1:根本 の場合が多いので調整が必要)
    // 通常の板ポリゴン草なら uv.y=1 が底部、uv.y=0 が先端
    float lerpVal = 1.0f - input.uv.y;
    float3 baseColor = input.color.rgb * 0.5f; // 根本
    float3 tipColor = input.color.rgb;        // 先端
    float3 albedo = lerp(baseColor, tipColor, lerpVal);
    
    // 2. Simple Lighting
    float3 N = normalize(input.normal);
    float3 finalColor = albedo * gAmbientColor;
    
    if (gDir[0].enabled) {
        float3 L = normalize(-gDir[0].dir);
        float diffuse = saturate(dot(N, L)) * 0.8f + 0.2f; // 半透明風のハーフランバート
        finalColor += albedo * gDir[0].color * diffuse;
    }
    
    return float4(finalColor, input.color.a);
}
