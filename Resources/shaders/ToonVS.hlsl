#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    VSOutput output;

    // 座標変換 (ワールド -> ビュー -> プロジェクション)
    // 頂点位置を画面上の座標に変換します
    output.svpos = mul(pos, mul(world, mul(view, projection)));
    
    // ワールド座標 (ライティング計算用)
    float4 worldPos = mul(pos, world);
    output.worldpos = worldPos;

    // 法線ベクトルの変換
    // 物体が回転しても正しく光を計算できるようにします
    output.normal = normalize(mul(float4(normal, 0), world).xyz);

    // UV座標 (テクスチャ用)
    output.uv = uv;

    return output;
}