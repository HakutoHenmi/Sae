//======================================================
// WallPS.hlsl
// 壁：ダークニュートラルグレー（非パステル）
// ・白混ぜ無し
// ・彩度ゼロ寄り
// ・背景として沈む
//======================================================
#include "Obj.hlsli"

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);

    // ----------------------------------
    // ① ベース色（かなり暗い・完全ニュートラル）
    // ----------------------------------
    float3 base = float3(0.42f, 0.42f, 0.42f);
    // さらに暗くするなら ↓
    // float3 base = float3(0.38f, 0.38f, 0.38f);

    float3 col = base;

    // ----------------------------------
    // ② 立体感のみ（最低限）
    // ----------------------------------
    float up = saturate(n.y);
    up = smoothstep(0.25f, 0.85f, up);

    // 明るくしすぎない
    float3 top = col * 1.02f;
    float3 side = col * 0.94f;
    col = lerp(side, top, up);

    return float4(col, 1.0f);
}
