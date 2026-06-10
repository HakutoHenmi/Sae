#include "Obj.hlsli"

// 最小: 標準の座標変換 + 必要な情報をVSOutputへ詰める
VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    VSOutput o;

    float4 worldPos = mul(pos, world);
    float4 worldN = normalize(mul(float4(normal, 0), world));

    o.svpos = mul(pos, mul(world, mul(view, projection)));
    o.worldpos = worldPos;
    o.normal = worldN.xyz;
    o.uv = uv;

    return o;
}
