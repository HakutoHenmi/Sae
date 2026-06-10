#include "Obj.hlsli"

// 入出力は VSOutput を使う（マニュアル準拠）:contentReference[oaicite:3]{index=3}
VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    VSOutput o;

    // 標準的な変換（サンプルと同じ形）:contentReference[oaicite:4]{index=4}
    o.svpos = mul(pos, mul(world, mul(view, projection)));
    o.worldpos = mul(pos, world);
    o.normal = normalize(mul(float4(normal, 0), world)).xyz;
    o.uv = uv;

    return o;
}
