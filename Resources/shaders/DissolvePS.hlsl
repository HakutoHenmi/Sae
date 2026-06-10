#include "Obj.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

// 簡単なノイズ関数
float hash(float2 p) { return frac(sin(dot(p, float2(12.9898,78.233))) * 43758.5453); }
float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(hash(i + float2(0.0, 0.0)), hash(i + float2(1.0, 0.0)), f.x),
                lerp(hash(i + float2(0.0, 1.0)), hash(i + float2(1.0, 1.0)), f.x), f.y);
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 texColor = tex.Sample(smp, uv);
    
    // 時間でディゾルブ閾値を変動ループ (0..1)
    float threshold = sin(time * 0.5) * 0.5 + 0.5;
    
    // オブジェクトローカル座標やUVベースのノイズ
    float n = noise(uv * 10.0 + input.worldpos.xy * 0.5);
    
    if(n < threshold) {
        discard; // ピクセルを破棄
    }
    
    // 燃え尽きエッジ（消失部分の境界）を光らせる
    float edgeWidth = 0.05;
    float isEdge = smoothstep(threshold, threshold + edgeWidth, n);
    
    // エッジ部分の色（オレンジ～赤）
    float3 edgeColor = float3(1.0, 0.4, 0.0) * 3.0; // 発光
    
    float3 baseColor = texColor.rgb * color.rgb;
    float3 finalColor = lerp(edgeColor, baseColor, isEdge);

    return float4(finalColor, texColor.a * color.a);
}
