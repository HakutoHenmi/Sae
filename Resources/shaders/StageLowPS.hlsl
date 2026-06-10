//======================================================
// StageLowPS.hlsl
// 低地：グリーン（#39855F）
// 方向性：Highと同じ「全体の流れ + 弱い揺らぎ」で、単色化を防ぐ
// ・UVは使わない（シームレス）
// ・worldpos.xz の斜め軸でどの床でも変化が出やすい
// ・Lowは主張しすぎないよう揺らぎ弱め
//======================================================
#include "Obj.hlsli"

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);

    // ----------------------------------
    // ① ベース色（指定のLow：#39855F）
    // ----------------------------------
    float3 baseGreen = float3(0.224f, 0.522f, 0.373f);

    // ----------------------------------
    // ② “ステージ全体の流れ”用の軸（XZ斜め）
    //    ※ Highと同じ思想：どの方向の床でも変化が出やすい
    // ----------------------------------
    float2 dir = normalize(float2(1.0f, 0.55f));
    float p = dot(i.worldpos.xz, dir);

    // ステージ全体レンジ（Highと揃えてOK）
    const float gMinP = -20.0f;
    const float gMaxP = 300.0f;

    float t_main = (p - gMinP) / max(0.0001f, (gMaxP - gMinP));
    t_main = saturate(t_main);
    t_main = smoothstep(0.10f, 0.90f, t_main);

    // ----------------------------------
    // ③ “単色化を防ぐ”弱い揺らぎ（Lowは控えめ）
    // ----------------------------------
    const float detailSpan = 14.0f; // Highより少し大きめ＝ゆっくり
    float t_detail = 0.5f + 0.5f * sin((p / detailSpan) * 6.2831853f);

    const float detailAmp = 0.08f; // Highより弱め（主張しすぎ防止）
    float t = t_main + (t_detail - 0.5f) * detailAmp;
    t = saturate(t);

    // ----------------------------------
    // ④ グラデで “白混ぜ量” を少しだけ動かす
    //    （色相は緑のまま、明度だけふわっと揺れる）
    // ----------------------------------
    float fogMixBase = 0.18f; // 元の値
    float fogMixVar = 0.06f; // 揺らぎ幅（大きすぎ注意）
    float fogMix = fogMixBase + (t - 0.5f) * fogMixVar;
    fogMix = saturate(fogMix);

    float3 col = lerp(baseGreen, 1.0.xxx, fogMix);

    // ----------------------------------
    // ⑤ 上面/側面の差（緑は影が汚くなりやすいので控えめ）
    // ----------------------------------
    float up = saturate(n.y);
    up = smoothstep(0.2f, 0.9f, up);

    float3 top = col * float3(1.06f, 1.08f, 1.06f);
    float3 side = col * float3(0.93f, 0.92f, 0.93f);

    col = lerp(side, top, up);
    return float4(col, 1.0f);
}

////明るいバージョン
////-------------------------------------------------
////======================================================
//// StageLowPS.hlsl
//// 低地：明るいパステルグリーン・グラデーション
//// 同じ構造：全体の流れ + 弱い揺らぎ
////======================================================
//#include "Obj.hlsli"

//float4 main(VSOutput i) : SV_TARGET
//{
//    float3 n = normalize(i.normal);

//    // ----------------------------------
//    // ① グラデの2色（どちらも“明るめ”）
//    // ----------------------------------
//    // 暗側（でも暗すぎない）
//    float3 colDark = float3(0.36f, 0.60f, 0.48f); // 明るめグリーン

//    // 明側（パステル寄り）
//    float3 colLight = float3(0.70f, 0.86f, 0.78f); // かなり明るいグリーン

//    // ----------------------------------
//    // ② ステージ全体の流れ（XZ斜め）
//    // ----------------------------------
//    float2 dir = normalize(float2(1.0f, 0.55f));
//    float p = dot(i.worldpos.xz, dir);

//    const float gMinP = -20.0f;
//    const float gMaxP = 300.0f;

//    float t_main = (p - gMinP) / max(0.0001f, (gMaxP - gMinP));
//    t_main = saturate(t_main);
//    t_main = smoothstep(0.10f, 0.90f, t_main);

//    // ----------------------------------
//    // ③ 弱い揺らぎ（単色ブロック潰し）
//    // ----------------------------------
//    const float detailSpan = 16.0f;
//    float t_detail = 0.5f + 0.5f * sin((p / detailSpan) * 6.2831853f);

//    const float detailAmp = 0.08f;
//    float t = t_main + (t_detail - 0.5f) * detailAmp;
//    t = saturate(t);

//    // ----------------------------------
//    // ④ 色グラデ（←ここが肝）
//    // ----------------------------------
//    float3 base = lerp(colDark, colLight, t);

//    // ----------------------------------
//    // ⑤ 白混ぜは“仕上げ”として少量だけ
//    // ----------------------------------
//    float fogMix = 0.10f; // 明るいので少なめ
//    float3 col = lerp(base, 1.0.xxx, fogMix);

//    // ----------------------------------
//    // ⑥ 上面／側面（やさしく）
//    // ----------------------------------
//    float up = saturate(n.y);
//    up = smoothstep(0.2f, 0.9f, up);

//    float3 top = col * float3(1.06f, 1.08f, 1.06f);
//    float3 side = col * float3(0.95f, 0.94f, 0.95f);

//    col = lerp(side, top, up);
//    return float4(col, 1.0f);
//}
