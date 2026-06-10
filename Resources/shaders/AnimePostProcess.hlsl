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

// ACES Tone Mapping
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Pseudo Bloom (1 pass simple box blur + threshold)
float3 PseudoBloom(float2 uv)
{
    float3 bloom = float3(0,0,0);
    float2 texSize = float2(1920.0f, 1080.0f); // 仮の解像度
    float2 texel = 1.0f / texSize;
    
    // クロス状の強いブラーで光の溢れを作る
    float weight[5] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};
    
    // しきい値を1.5fに上げて、本当に明るい場所だけBloomさせる。強度も0.3fに下げる
    float bloomThreshold = 1.3f;
    float bloomIntensity = 0.3f;
    
    for(int i = -4; i <= 4; ++i) {
        float2 offset = float2(i, 0) * texel * 3.0f;
        float3 c = gScene.Sample(gSmp, uv + offset).rgb;
        float brightness = dot(c, float3(0.2126, 0.7152, 0.0722));
        if(brightness > bloomThreshold) bloom += c * weight[abs(i)] * bloomIntensity;
        
        offset = float2(0, i) * texel * 3.0f;
        c = gScene.Sample(gSmp, uv + offset).rgb;
        brightness = dot(c, float3(0.2126, 0.7152, 0.0722));
        if(brightness > bloomThreshold) bloom += c * weight[abs(i)] * bloomIntensity;
    }
    
    return bloom;
}

// Color Grading (Wuthering Waves style)
float3 ColorGrade(float3 color)
{
    // 輝度計算
    float luminance = dot(color, float3(0.299, 0.587, 0.114));
    
    // シャドウは少し青く冷たく、ハイライトは暖かく（シネマティックな色調）
    float3 shadowTint = float3(0.9f, 0.95f, 1.0f); 
    float3 highlightTint = float3(1.0f, 0.98f, 0.95f);
    
    color = lerp(color * shadowTint, color * highlightTint, luminance);
    
    // コントラスト調整 (強すぎると明るくなるため少し抑える)
    color = (color - 0.5f) * 1.05f + 0.5f;
    
    // 彩度調整
    float3 invGray = dot(color, float3(0.299, 0.587, 0.114)).xxx;
    color = lerp(invGray, color, 1.1f);
    
    return max(color, 0.0f);
}

float4 main(PSIn i) : SV_TARGET
{
    float2 uv = i.uv;
    
    // 1. Base Scene Color
    float3 col = gScene.Sample(gSmp, uv).rgb;
    
    // 2. Simple Bloom (高輝度部分の抽出と加算)
    float3 bloom = PseudoBloom(uv);
    col += bloom;
    
    // 3. Color Grading (色調補正)
    col = ColorGrade(col);
    
    // 追加: 露出の調整（全体が明るすぎるのを防ぐ）
    float exposure = 0.8f;
    col *= exposure;
    
    // 4. ACES Tone mapping (HDRからLDRへマッピング)
    col = ACESFilm(col);
    
    // 5. Vignette効果等の既存パラメータの適用
    float2 d = uv - 0.5f;
    col *= saturate(1.0f - dot(d,d) * gVignette);
    
    // ノイズやスキャンラインが必要な場合はここで適用(今回はアニメ調のため弱めるか除外)
    col -= sin(uv.y * 900.0f).xxx * gScanline * 0.2f;
    
    return float4(col, 1.0f);
}
