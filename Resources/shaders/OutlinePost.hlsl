// Resources/shaders/OutlinePost.hlsl
// ポストエフェクト: エッジ検出アウトライン（ラプラシアンフィルタ）
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
    float2 texelSize = float2(1.0 / 1280.0, 1.0 / 720.0);

    // 周囲8近傍の輝度を取得
    float luTL = Luminance(gScene.Sample(gSmp, i.uv + float2(-1, -1) * texelSize).rgb);
    float luTC = Luminance(gScene.Sample(gSmp, i.uv + float2( 0, -1) * texelSize).rgb);
    float luTR = Luminance(gScene.Sample(gSmp, i.uv + float2( 1, -1) * texelSize).rgb);
    float luML = Luminance(gScene.Sample(gSmp, i.uv + float2(-1,  0) * texelSize).rgb);
    float luMC = Luminance(gScene.Sample(gSmp, i.uv).rgb);
    float luMR = Luminance(gScene.Sample(gSmp, i.uv + float2( 1,  0) * texelSize).rgb);
    float luBL = Luminance(gScene.Sample(gSmp, i.uv + float2(-1,  1) * texelSize).rgb);
    float luBC = Luminance(gScene.Sample(gSmp, i.uv + float2( 0,  1) * texelSize).rgb);
    float luBR = Luminance(gScene.Sample(gSmp, i.uv + float2( 1,  1) * texelSize).rgb);

    // ラプラシアンフィルタ (8方向)
    //  -1 -1 -1
    //  -1  8 -1
    //  -1 -1 -1
    float laplacian = -luTL - luTC - luTR
                    - luML + 8.0 * luMC - luMR
                    - luBL - luBC - luBR;

    float edge = saturate(abs(laplacian));

    // gDistortion でエッジ強度を制御
    float strength = max(1.0, 1.0 + gDistortion * 5.0);
    edge = saturate(edge * strength);

    // 元の色を取得
    float3 sceneColor = gScene.Sample(gSmp, i.uv).rgb;

    // エッジ部分を暗くする（アウトライン描画）
    // gChromaShift > 0 ならエッジのみ表示モード
    float3 col;
    if (gChromaShift > 0.5)
    {
        // エッジのみ表示（白い線 on 黒背景）
        col = edge.xxx;
    }
    else
    {
        // 元の画像にアウトラインを重ねる
        col = sceneColor * (1.0 - edge);
    }

    // Vignette
    float2 d = i.uv - 0.5;
    col *= saturate(1.0 - dot(d, d) * gVignette);

    return float4(Saturate3(col), 1.0);
}
