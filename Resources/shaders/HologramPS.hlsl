#include "Obj.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    
    // スキャンライン
    float scanline = sin(uv.y * 800.0 - time * 20.0) * 0.5 + 0.5;
    
    // クロマティック・アベレーション (色収差)
    float shift = 0.02 * sin(time * 5.0);
    float r = tex.Sample(smp, uv + float2(shift, 0)).r;
    float g = tex.Sample(smp, uv).g;
    float b = tex.Sample(smp, uv - float2(shift, 0)).b;
    float3 albedo = float3(r, g, b) * color.rgb;

    // リムライト発光
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);
    float rim = 1.0 - saturate(dot(V, N));
    float rimPower = pow(rim, 3.0) * 2.0;
    
    // 強調
    float3 finalColor = albedo * (1.0 + scanline * 0.5) + float3(0.0, 0.5, 1.0) * rimPower;

    // 半透明
    float alpha = color.a * (0.5 + scanline * 0.3 + rimPower);

    return float4(finalColor, saturate(alpha));
}
