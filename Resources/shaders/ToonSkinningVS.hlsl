#include "Obj.hlsli"

// スキニング用定数バッファ
cbuffer CBBone : register(b3)
{
    matrix gBones[128];
};

// スキニング対応トゥーン頂点シェーダー
VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD,
              float4 weights : WEIGHTS, uint4 indices : BONES)
{
    VSOutput output;

    // スキニング行列の合成
    matrix skinMat =
        gBones[indices.x] * weights.x +
        gBones[indices.y] * weights.y +
        gBones[indices.z] * weights.z +
        gBones[indices.w] * weights.w;

    float4 skinnedPos = mul(pos, skinMat);
    float3 skinnedNrm = mul(float4(normal, 0.0f), skinMat).xyz;

    // ワールド座標変換
    float4 worldPos = mul(skinnedPos, world);
    output.worldpos = worldPos;
    output.normal = normalize(mul(float4(skinnedNrm, 0), world).xyz);
    output.svpos = mul(worldPos, mul(view, projection));
    output.uv = uv;

    return output;
}
