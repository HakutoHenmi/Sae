#include "Obj.hlsli"

// アウトライン用頂点シェーダー
// 法線方向に頂点を膨らませて、前面カリングと組み合わせることでアウトラインを描画する

static const float outlineWidth = 0.008f; // アウトラインの太さ（細め）

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    VSOutput output;

    // ワールド座標変換
    float4 worldPos = mul(pos, world);
    float3 worldNormal = normalize(mul(float4(normal, 0), world).xyz);

    // クリップ空間（画面空間）での位置を計算
    float4 clipPos = mul(worldPos, mul(view, projection));

    // 法線をクリップ空間へ変換
    float3 clipNormal = mul(worldNormal, (float3x3)mul(view, projection));
    float2 offset = normalize(clipNormal.xy);

    // アスペクト比や距離に影響されないよう、画面のXY平面上で押し出す
    // w成分を掛けることで遠くの物体でも一定の太さを保つ
    clipPos.xy += offset * outlineWidth * clipPos.w;

    // 出力
    output.svpos = clipPos;
    output.worldpos = worldPos;
    output.normal = worldNormal;
    output.uv = uv;

    return output;
}
