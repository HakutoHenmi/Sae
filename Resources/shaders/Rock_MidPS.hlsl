#include "Obj.hlsli"

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);

    float up = saturate(n.y);
    up = smoothstep(0.2, 0.95, up);

// いまの色を low とする
    float3 low = float3(0.12, 0.06, 0.30); // 今の紫寄り
    float3 high = float3(0.22, 0.28, 0.50); // 少し青を足す

    float3 col = lerp(low, high, up);


    // Inspector color で全体トーン調整できる余地を残す
    col *= color.rgb;

    return float4(col, 1.0);
}
