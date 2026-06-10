#include "Obj.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    
    // バリア状のスクロールテクスチャ（六角形や模様の代わり）
    float2 scrollUV = uv * 3.0 + float2(time * 0.2, time * 0.5);
    float4 texColor = tex.Sample(smp, scrollUV); // 仮に既存のテクスチャを模様として利用

    // ジオメトリの波打ちエフェクト
    float wave = sin(uv.y * 20.0 - time * 5.0) * 0.5 + 0.5;
    wave *= sin(uv.x * 20.0 + time * 3.0) * 0.5 + 0.5;

    // 強烈なフレネル（輪郭発光）
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);
    float rim = 1.0 - saturate(dot(V, N));
    float shieldRim = pow(rim, 2.0); // 輪郭だけ明るく
    
    float3 baseColor = color.rgb * 2.0; // 発光色
    float3 finalColor = baseColor * shieldRim + baseColor * wave * 0.5;
    
    // 中は透けている
    float alpha = shieldRim + wave * 0.3;

    return float4(finalColor, saturate(alpha * color.a));
}
