// Resources/shaders/CRTPost.hlsl
#include "PostProcessCommon.hlsli"

Texture2D gScene : register(t0);
SamplerState gSmp : register(s0);

// Engine から送る
cbuffer CBPost : register(b0)
{
    float gTime;
    float gNoiseStrength; // 基本ノイズ（小さめ推奨）
    float gDistortion;
    float gChromaShift;
    float gVignette;
    float gScanline;

    float gSan; // 0..1
    float pad0;
};

struct PSIn
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// --------------------
// 調整しやすいように “係数” をまとめる
// --------------------
struct FxParams
{
    float san; // 0..1 (整形済み)
    float kDist;
    float kNoiseAdd; // SANで追加するノイズ倍率
    float kChroma;
    float kVhs;
};

static FxParams MakeFxParams(float san01)
{
    FxParams p;
    // 変化を自然に（0付近を抑え、後半で急に効く）
    p.san = smoothstep(0.0, 1.0, saturate(san01));

    // 既存の倍率（必要ならここで調整）
    p.kDist = (1.0 + p.san * 2.6);
    p.kChroma = (1.0 + p.san * 2.3);
    p.kVhs = (0.25 + p.san * 1.00);

    // ★重要：通常時ノイズは抑え、SANで強く増やすための倍率
    // ここを上げると “近いほど荒れる” が強くなる
    p.kNoiseAdd = (p.san * p.san) * 6.0; // 二乗で後半に寄せる + 最大6倍
    return p;
}

// --------------------
// Grain + Tape を “通常” と “SAN追加” に分けて合成
// --------------------
static float3 ApplyNoise(float2 uv, float time, float baseStrength, float sanAddStrength)
{
    // 粒状ノイズ（細かい）
    float g1 = Noise2D(uv * 900.0 + time * 12.0);
    // 低周波ノイズ（ムラ）
    float g2 = FBM(uv * 60.0 + time * 0.8);

    // centered
    float grainFine = (g1 - 0.5);
    float grainLow = (g2 - 0.5) * 0.6;

    float grain = grainFine + grainLow;

    // 通常時（かなり弱め）
    float base = grain * baseStrength;

    // SAN追加（強く）
    float san = grain * sanAddStrength;

    return (base + san).xxx;
}

float4 main(PSIn i) : SV_TARGET
{
    float2 uv = i.uv;

    // --------------------
    // SAN を作る（0..1）
    // --------------------
    FxParams fx = MakeFxParams(gSan);

    // --------------------
    // VHS/CRT 変形
    // --------------------
    uv.y += VHS_VerticalJump(gTime, fx.kVhs);

    uv = Barrel(uv, 0.65 + fx.san * 0.15);
    uv = DistortCRT(uv, gTime, gDistortion * fx.kDist);

    uv.x += VHS_LineWobble(uv, gTime, fx.kVhs);
    uv.x += LineJitter(uv, gTime, 0.18 * fx.kVhs);

    float2 uvc = ClampUV(uv);

    // --------------------
    // 色にじみ + RGB分離
    // --------------------
    float3 smear = VHS_ChromaSmear(gScene, gSmp, uvc, fx.kVhs);
    float2 shift = float2(gChromaShift * fx.kChroma, 0.0);
    float3 chroma = SampleChroma(gScene, gSmp, uvc, shift);
    float3 col = lerp(chroma, smear, 0.55);

    // --------------------
    // Scanline
    // --------------------
    col *= Scanline(i.uv, gScanline);

    // --------------------
    // ★ノイズ：通常時弱め / SANで強め
    // --------------------
    // 通常時ノイズをさらに抑えたいなら 0.25 → 0.15 とかに下げる
    float baseNoise = gNoiseStrength * 0.25;

    // SANで増える分：最大で baseNoise + gNoiseStrength * (kNoiseAdd) くらいになる
    float sanNoise = gNoiseStrength * fx.kNoiseAdd;

    col += ApplyNoise(i.uv, gTime, baseNoise, sanNoise);

    // テープ帯ノイズ（SANで増幅）
    col += VHS_TapeBand(i.uv, gTime, fx.kVhs).xxx * (0.6 + fx.san * 1.2);

    // --------------------
    // Vignette（SANで強化）
    // --------------------
    float v = Vignette(i.uv, gVignette + fx.san * 0.35);
    col *= v;

    // --------------------
    // SAN: 端で色落ち + 微暗転
    // --------------------
    float edge = saturate((v - 0.2) / 0.8);
    float lum = Luminance(col);
    col = lerp(lum.xxx * 0.85, col, edge);
    col *= (1.0 - fx.san * 0.10);

    return float4(Saturate3(col), 1.0);
}
