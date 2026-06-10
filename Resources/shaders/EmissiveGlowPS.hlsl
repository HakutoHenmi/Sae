#include "Obj.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = float2(
        input.uv.x * m_uv_scale.x + m_uv_offset.x,
        input.uv.y * m_uv_scale.y + m_uv_offset.y
    );
    float4 texcolor = tex.Sample(smp, uv);
    float3 albedo = texcolor.rgb * color.rgb;
    
    // 発光の強さ (輝度を倍率で上げることでBloomの閾値を超える)
    float emissionStrength = 5.0f;
    
    // リムライト (輪郭をより強く光らせる)
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);
    float rim = 1.0 - saturate(dot(V, N));
    rim = smoothstep(0.6, 1.0, rim);
    
    float3 finalColor = albedo * emissionStrength + (albedo * rim * emissionStrength * 2.0);

    return float4(finalColor, texcolor.a * color.a);
}
