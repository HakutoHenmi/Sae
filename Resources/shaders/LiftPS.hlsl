//======================================================
// LiftPS.hlsl
// リフト：暗めイエロー（くすんだ工業系）
// ・黄色ベースだが暗め
// ・床/壁の配色を邪魔しない
// ・UVは使わない（シームレス）
//======================================================
#include "Obj.hlsli"

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);

    // -----------------------------
    // ① ベース：暗めイエロー（安全色っぽいが沈める）
    // -----------------------------
    // 明るすぎると主役を食うので、最初から暗い黄土寄りに
    float3 base = float3(0.62f, 0.56f, 0.18f); // dark yellow

    // -----------------------------
    // ② ノッペリ防止の“超弱い”明度差（縞になりにくい）
    //    ※ worldpos は Obj.hlsli の VSOutput.worldpos を使用
    // -----------------------------
    float2 dir = normalize(float2(1.0f, 0.55f));
    float p = dot(i.worldpos.xz, dir);

    const float span = 40.0f; // 大きいほどゆっくり変化
    float w = 0.5f + 0.5f * sin((p / span) * 6.2831853f);

    // 明るくしすぎない（暗→少し暗）
    float shade = lerp(0.92f, 1.00f, w);
    float3 col = base * shade;

    // -----------------------------
    // ③ 立体感（弱め：主張しすぎ防止）
    // -----------------------------
    float up = saturate(n.y);
    up = smoothstep(0.25f, 0.85f, up);

    float3 top = col * 1.04f;
    float3 side = col * 0.93f;
    col = lerp(side, top, up);

    return float4(col, 1.0f);
}
