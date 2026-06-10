// Resources/shaders/RandomPost.hlsl
// ポストエフェクト: ランダムノイズ
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

    // --- ランダムノイズ効果 ---

    // 1. 細かい白色ノイズ（フィルムグレイン風）
    float grain = Hash12(i.uv * 1000.0 + gTime * 7.3);
    grain = (grain - 0.5) * 2.0; // -1 ~ 1

    // 2. ブロックノイズ（デジタルグリッチ風）
    float2 blockUV = floor(i.uv * 40.0) / 40.0; // ブロック化されたUV
    float blockNoise = Hash12(blockUV + floor(gTime * 8.0));
    float blockStrength = step(0.92, blockNoise); // ランダムな位置にブロックが出現

    // 3. UV歪みノイズ
    float2 noiseOffset;
    noiseOffset.x = (Hash12(float2(floor(i.uv.y * 200.0), gTime * 3.0)) - 0.5) * 0.01;
    noiseOffset.y = (Hash12(float2(floor(i.uv.x * 200.0), gTime * 5.0 + 100.0)) - 0.5) * 0.01;

    // gDistortion でノイズ強度を制御
    float intensity = 0.3 + gDistortion * 0.5;

    // UV歪みを適用して再サンプリング
    float2 noisyUV = i.uv + noiseOffset * intensity;
    float3 noisyCol = gScene.Sample(gSmp, saturate(noisyUV)).rgb;

    // ブロックノイズがある場所は色を乱す
    float3 blockCol = Hash12(blockUV + gTime).xxx;
    col = lerp(noisyCol, blockCol, blockStrength * intensity * 0.5);

    // グレインノイズを加算
    float noiseAmount = gNoiseStrength + intensity * 0.15;
    col += grain * noiseAmount;

    // Vignette
    float2 d = i.uv - 0.5;
    col *= saturate(1.0 - dot(d, d) * gVignette);

    return float4(Saturate3(col), 1.0);
}
