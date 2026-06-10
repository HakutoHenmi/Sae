#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    VSOutput o;
    float4 worldPos = mul(pos, world);
    o.svpos = mul(worldPos, mul(view, projection));
    o.worldpos = worldPos;
    o.normal = normalize(mul(float4(normal, 0), world)).xyz;
    o.uv = uv;
    return o;
}
