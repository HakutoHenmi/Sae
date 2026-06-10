// StylizedGrassVS.hlsl
#include "Grass.hlsli"

VSOutput main(VSInput input) {
    VSOutput output;
    InstanceData instance = gInstanceData[input.instanceID];
    
    float4 worldPos = mul(input.pos, instance.world);
    
    // 1. Wind Animation
    // 頂点の高さ(ローカル pos.y)に基づいて揺れを適用する
    // 草の根元(y=0)は動かさず、先端ほど大きく揺らす
    float heightWeight = saturate(input.pos.y); 
    
    float3 windDir = normalize(float3(gWindParams.x, 0, gWindParams.y));
    float windSpeed = gWindParams.z;
    float windStrength = gWindParams.w;
    
    // サイン波による揺れ
    float sway = sin(gTime * windSpeed + (worldPos.x + worldPos.z) * 0.5f) * windStrength;
    worldPos.xyz += windDir * sway * heightWeight;
    
    // 2. Player Interaction
    // プレイヤーとの距離に応じて草を押し出す
    float3 toPlayer = worldPos.xyz - gPlayerPos;
    float distToPlayer = length(toPlayer);
    float radius = 1.5f; // インタラクション半径
    
    if (distToPlayer < radius) {
        float pushWeight = saturate(1.0f - (distToPlayer / radius));
        worldPos.xyz += normalize(toPlayer) * pushWeight * 1.0f * heightWeight;
    }
    
    output.worldPos = worldPos.xyz;
    output.svpos = mul(float4(worldPos.xyz, 1.0f), gViewProj);
    output.normal = normalize(mul(float4(input.normal, 0.0f), instance.world).xyz);
    output.uv = input.uv;
    output.color = instance.color;
    
    return output;
}
