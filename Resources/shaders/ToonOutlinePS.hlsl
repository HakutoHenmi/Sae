#include "Obj.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

// アウトライン用ピクセルシェーダー
// テクスチャの色を元にして、自然な暗い輪郭色（Colored Outline）を作る
float4 main(VSOutput input) : SV_TARGET
{
    // テクスチャのサンプリング
    float4 texColor = tex.Sample(smp, input.uv);
    
    // ベースカラーの取得
    float3 baseColor = texColor.rgb * color.rgb;

    // アウトラインの色味（元の色の明度を落とし、少し青み/紫みを足して影っぽくする）
    // 完全な黒ではなく、ベースカラーに馴染むようにする
    float3 outlineTint = float3(0.3f, 0.25f, 0.4f); // 暗い紫寄りの影色
    float3 finalOutlineColor = baseColor * outlineTint;

    // 半透明のピクセルはアウトラインも抜くようアルファをそのまま使う
    return float4(finalOutlineColor, texColor.a * color.a);
}
