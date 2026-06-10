#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    VSOutput output;

    float4 worldPos = mul(pos, world);
    float3 worldN = normalize(mul(float4(normal, 0), world)).xyz;

    output.svpos = mul(pos, mul(world, mul(view, projection)));
    output.worldpos = worldPos;
    output.normal = worldN;
    output.uv = uv;

    return output;
}
