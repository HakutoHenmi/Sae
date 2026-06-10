#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    // 標準変換（資料サンプルと同系統）
    float4 worldPos = mul(pos, world);
    float4 worldNormal = normalize(mul(float4(normal, 0), world));

    VSOutput o;
    o.svpos = mul(pos, mul(world, mul(view, projection)));
    o.worldpos = worldPos;
    o.normal = worldNormal.xyz;
    o.uv = uv;
    return o;
}
