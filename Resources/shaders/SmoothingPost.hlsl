// Resources/shaders/SmoothingPost.hlsl
// ポストエフェクト: 平滑化フィルタ（ボックスフィルタ / 平均化フィルタ）
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
    // 解像度（定数）
    float2 texelSize = float2(1.0 / 1280.0, 1.0 / 720.0);

    // カーネルサイズ: gDistortion で制御 (0=ぼかしなし, 大=強いぼかし)
    // デフォルトでは3x3ボックスフィルタ
    float radius = max(1.0, 1.0 + gDistortion * 3.0);
    int iRadius = (int)radius;
    iRadius = clamp(iRadius, 1, 5); // 最大11x11

    float3 sum = float3(0, 0, 0);
    float count = 0;

    // ボックスフィルタ: 周囲のピクセルを均等に平均
    for (int y = -iRadius; y <= iRadius; ++y)
    {
        for (int x = -iRadius; x <= iRadius; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            sum += gScene.Sample(gSmp, i.uv + offset).rgb;
            count += 1.0;
        }
    }
    float3 col = sum / count;

    // Vignette
    float2 d = i.uv - 0.5;
    col *= saturate(1.0 - dot(d, d) * gVignette);

    return float4(Saturate3(col), 1.0);
}
