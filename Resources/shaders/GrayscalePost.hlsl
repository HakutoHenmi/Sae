// Resources/shaders/GrayscalePost.hlsl
// ポストエフェクト: グレースケール変換
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
    float3 col = gScene.Sample(gSmp, i.uv).rgb;

    // ITU-R BT.709 輝度係数によるグレースケール変換
    float gray = Luminance(col);

    // gDistortion を変換強度として使用 (0=カラー, 1=完全グレー)
    float strength = saturate(gDistortion + 1.0);
    col = lerp(col, gray.xxx, strength);

    // Vignette
    float2 d = i.uv - 0.5;
    col *= saturate(1.0 - dot(d, d) * gVignette);

    return float4(Saturate3(col), 1.0);
}
