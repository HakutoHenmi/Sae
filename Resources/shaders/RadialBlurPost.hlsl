// Resources/shaders/RadialBlurPost.hlsl
// ポストエフェクト: ラジアルブラー（放射状ぼかし）
#include "PostProcessCommon.hlsli"

Texture2D gScene : register(t0);
SamplerState gSmp : register(s0);

cbuffer CBPost : register(b0)
{
    float gTime;
    float gNoiseStrength;
    float gDistortion;
    float gChromaShift;
    float gVignette;
    float gScanline;
    float gSan;
    float pad0;
};

struct PSIn
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn i) : SV_TARGET
{
    // ブラーの中心（画面中心）
    float2 center = float2(0.5, 0.5);
    
    // 中心からの方向ベクトル
    float2 dir = i.uv - center;
    float dist = length(dir);

    // gDistortion でブラー強度を制御
    float blurStrength = 0.02 + gDistortion * 0.05;

    // サンプル数
    static const int SAMPLES = 16;

    float3 col = float3(0, 0, 0);
    float totalWeight = 0;

    [unroll]
    for (int s = 0; s < SAMPLES; ++s)
    {
        float t = (float)s / (float)(SAMPLES - 1); // 0..1
        // 中心方向に向かってサンプリング
        float2 offset = dir * t * blurStrength;
        float2 sampleUV = i.uv - offset;

        // 中心に近いほど重みが大きい
        float weight = 1.0 - t * 0.5;
        col += gScene.Sample(gSmp, saturate(sampleUV)).rgb * weight;
        totalWeight += weight;
    }
    col /= totalWeight;

    // Vignette
    float2 d = i.uv - 0.5;
    col *= saturate(1.0 - dot(d, d) * gVignette);

    return float4(Saturate3(col), 1.0);
}
