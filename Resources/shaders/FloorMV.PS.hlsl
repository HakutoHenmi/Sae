#include "Obj.hlsli"

// Texture は使わない（床はベタ色でMV寄せ）
// Texture2D tex : register(t0);
// SamplerState smp : register(s0);

float3 Overlay(float3 baseC, float3 overC)
{
    // MV風の柔らかい乗算/スクリーン
    return lerp(
        2.0 * baseC * overC,
        1.0 - 2.0 * (1.0 - baseC) * (1.0 - overC),
        step(0.5, baseC)
    );
}

float4 main(VSOutput input) : SV_TARGET
{
    // --- Monument Valley っぽい「パステル化」 ---
    // 1) ベース色は Inspector の color だけ
    float3 baseC = saturate(color.rgb);

    // 2) 彩度を落として明度を上げる（ふわっと）
    float luma = dot(baseC, float3(0.299, 0.587, 0.114));
    float3 pastel = lerp(baseC, luma.xxx, 0.45); // 彩度↓
    pastel = lerp(pastel, 1.0.xxx, 0.15); // 明度↑

    // 3) ほんのりグラデ（高さで変化）→ 幻想感
    float h = saturate((input.worldpos.y + 5.0) / 20.0);
    float3 gradA = float3(0.62, 0.84, 0.78); // ミント寄り
    float3 gradB = float3(0.78, 0.72, 0.89); // ラベンダー寄り
    float3 grad = lerp(gradA, gradB, h);

    float3 outC = Overlay(pastel, grad);

    // --- ON/OFFで半透明にしたい場合 ---
    // texture alpha は無いので color.a のみを使う
    float alpha = saturate(color.a);

    return float4(outC, alpha);
}
