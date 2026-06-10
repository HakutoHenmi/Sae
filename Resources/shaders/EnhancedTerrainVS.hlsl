// EnhancedTerrainVS.hlsl
#include "EnhancedTerrain.hlsli"

VSOutput main(VSInput input) {
    InstanceData data = gInstances[input.instanceID];
    VSOutput output;
    float4 worldPos = mul(input.pos, data.world);
    output.worldPos = worldPos.xyz;
    output.svpos = mul(worldPos, gViewProj);
    output.normal = normalize(mul(float4(input.normal, 0.0f), data.world).xyz);
    output.uv = input.uv;
    output.color = data.color;
    return output;
}
