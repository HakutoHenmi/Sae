#include "Obj.hlsli"

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float4 t = tex.Sample(smp, input.uv);
    float4 c = t * color;

    bool isOff = (color.a < 0.999f);

    if (isOff)
    {
        // --------------------
        // OFFF”¼“§–¾{–³ŒøŠ´
        // --------------------
        float gray = dot(c.rgb, float3(0.299, 0.587, 0.114));
        c.rgb = lerp(c.rgb, float3(gray, gray, gray), 0.65);

        float blink = 0.85 + 0.15 * sin(time * 6.28318 * 1.5);
        c.rgb *= blink;
    }
    else
    {
        // --------------------
        // ONFF‚ð”Z‚­‚·‚é
        // --------------------
        float boost = 1.25; // ©‚±‚±‚Å”Z‚³’²®
        c.rgb = saturate(c.rgb * boost);
    }

    return c;
}

