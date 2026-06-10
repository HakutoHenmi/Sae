#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    // 揺れパラメータ
    float amp = 0.5f; // 揺れ幅
    float freq = 2.0f; // 揺れの速さ
    float wave = 3.0f; // 波の細かさ
    
    // 頂点位置を時間で揺らす (XとZをずらす)
    pos.x += sin(pos.y * wave + time * freq) * amp * 0.1f;
    pos.z += cos(pos.y * wave + time * freq) * amp * 0.1f;

    // 通常の座標変換
    float4 worldPos = mul(pos, world);
    float4 worldNormal = normalize(mul(float4(normal, 0), world));

    VSOutput output;
    output.svpos = mul(pos, mul(world, mul(view, projection)));
    output.worldpos = worldPos;
    output.normal = worldNormal.xyz;
    output.uv = uv;

    return output;
}