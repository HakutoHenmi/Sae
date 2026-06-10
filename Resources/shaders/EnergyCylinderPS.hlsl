#include "Obj.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    // 上向き（円柱のフタ）を完全に描かない
    if (input.normal.y > 0.8f)
    {
        discard;
    }
    
    // -------------------------
    // 高さ（ワールドY基準）
    // -------------------------
    float h = input.worldpos.y;

    // 下から上へ流れる
    float flow = sin(h * 8.0f - time * 6.0f);
    flow = flow * 0.5f + 0.5f;
    flow = pow(flow, 3.0f);

    // -------------------------
    // 中心からの距離（半径）
    // -------------------------
    float3 center = float3(world._41, world._42, world._43);
    float dist = length(input.worldpos.xz - center.xz);

    // 中心ほど強い（＝柱の“芯”）
    float radial = saturate(1.0f - dist * 3.0f);
    radial = pow(radial, 2.0f);

    // -------------------------
    // 根元ブースト（キューブ接触部）
    // -------------------------
    float base = saturate(1.0f - abs(h - center.y) * 4.0f);

    // -------------------------
    // フレネルで縁をちょい強調
    // -------------------------
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);
    float fresnel = pow(1.0f - saturate(dot(N, V)), 2.5f);

    // -------------------------
    // エネルギー合成
    // -------------------------
    float energy =
        flow * 0.8f +
        radial * 1.4f +
        base * 1.2f +
        fresnel * 0.4f;

    // 脈動
    energy *= (0.8f + 0.2f * sin(time * 5.0f));


    float alpha = saturate(energy);
    float3 rgb = color.rgb * energy * 1.5f;

    return float4(rgb, alpha);
}
