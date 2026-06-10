#include "Obj.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    // --- 基本 ---
    float2 uv = input.uv;

    // UV中心からの距離（リング用）
    float2 p = uv - float2(0.5f, 0.5f);
    float r = length(p); // 0..~0.7

    // --- リング波（エネルギーの「うねり」）---
    // r方向に流れる波：sinで擬似的にエネルギーが走る
    float wave = sin((r * 30.0f) - (time * 10.0f));
    wave = wave * 0.5f + 0.5f; // 0..1
    wave = pow(wave, 3.0f); // コントラスト強め

    // --- 脈動（発光が呼吸する）---
    float pulse = sin(time * 6.0f) * 0.5f + 0.5f; // 0..1
    pulse = 0.6f + 0.4f * pulse;

    // --- 外周を強める（Fresnelっぽい縁発光）---
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz); // Obj.hlsliの cameraPos を想定
    float fresnel = pow(1.0f - saturate(dot(N, V)), 3.0f); // 0..1

    // --- 中心を強く（エネルギー核）---
    float core = saturate(1.0f - (r * 2.2f));
    core = pow(core, 2.0f);

    // --- 合成 ---
    // color は Inspector の色（b1）をそのまま“発光色”として使う
    float energy = (wave * 0.8f + core * 1.2f + fresnel * 1.0f) * pulse;

    float3 rgb = color.rgb * energy;

    // 透明っぽくしたいなら alpha を energy に寄せる（不透明のままでもOK）
    float a = saturate(0.2f + energy);

    return float4(rgb, a);
}
