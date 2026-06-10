////======================================================
//// StageHighPS.hlsl
//// 2枚目の色（コーラル）に固定 + “グラデしないブロック”対策入り
////======================================================
//#include "Obj.hlsli"

//float4 main(VSOutput i) : SV_TARGET
//{
//    float3 n = normalize(i.normal);

//    // -----------------------------
//    // ① 2枚目の画像と同じグラデ色
//    // -----------------------------
//    float3 colTop = float3(0.878f, 0.478f, 0.447f); // #E07A72
//    float3 colBot = float3(0.608f, 0.416f, 0.416f); // #9B6A6A

//    // -----------------------------
//    // ② “ステージ全体の流れ” になる軸（XZの斜め）
//    //   ※Xだけ/Zだけより、どの方向の床でも変化しやすい
//    // -----------------------------
//    float2 dir = normalize(float2(1.0f, 0.55f));
//    float p = dot(i.worldpos.xz, dir);

//    // ステージ全体レンジ（まず仮）
//    // ※グラデ弱い→maxを小さく / 強い→maxを大きく
//    const float gMinP = -20.0f;
//    const float gMaxP = 300.0f;

//    float t_main = (p - gMinP) / max(0.0001f, (gMaxP - gMinP));
//    t_main = saturate(t_main);
//    t_main = smoothstep(0.10f, 0.90f, t_main);

//    // -----------------------------
//    // ③ “グラデしないブロック”潰し：弱い補助グラデ
//    //   - UV不使用（シームレス）
//    //   - 影響は小さく、でも確実に変化が出る
//    // -----------------------------
//    // 周期（大きいほどゆっくり、でも「無変化」を潰すには少しだけ速め）
//    const float detailSpan = 12.0f; // 8〜20で調整
//    float t_detail = 0.5f + 0.5f * sin((p / detailSpan) * 6.2831853f);

//    // 補助の強さ（上品にするなら小さめ）
//    const float detailAmp = 0.12f; // 0.08〜0.18で調整

//    // 中心(0.5)からのズレだけ足す（極端な縞になりにくい）
//    float t = t_main + (t_detail - 0.5f) * detailAmp;
//    t = saturate(t);

//    float3 base = lerp(colBot, colTop, t);

//    // パステル化（魚と共通点）
//    float fogMix = 0.16f; // 濃い→0.12 / 霧→0.22
//    float3 col = lerp(base, 1.0.xxx, fogMix);

//    // 立体感（控えめ）
//    float up = saturate(n.y);
//    up = smoothstep(0.2f, 0.9f, up);
//    col *= lerp(0.97f, 1.03f, up);

//    return float4(col, 1.0f);
//}






//======================================================
// StageHighPS.hlsl
// パステル水色 → コーラル
// ・残す：ステージ全体グラデ（t_main）
// ・消す：一ブロック内の補助グラデ（t_detail）
//======================================================
#include "Obj.hlsli"
#include "ProceduralPaper.hlsli"

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);

    // 端の2色：パステル水色 → コーラル
    float3 colAqua = float3(0.72f, 0.90f, 0.94f);
    float3 colCoral = float3(0.878f, 0.478f, 0.447f);

    // ステージ全体の流れ（XZ斜め）
    float2 dir = normalize(float2(1.0f, 0.55f));
    float p = dot(i.worldpos.xz, dir);

    const float gMinP = -20.0f;
    const float gMaxP = 300.0f;

    float t_main = (p - gMinP) / max(0.0001f, (gMaxP - gMinP));
    t_main = saturate(t_main);
    t_main = smoothstep(0.10f, 0.90f, t_main);

    // ★ここだけ：ステージ全体グラデだけ残す
    float t = t_main;

    // 色：水色 → コーラル
    float3 base = lerp(colAqua, colCoral, t);

    // パステル化（魚との共通点）
    float fogAqua = 0.10f;
    float fogCoral = 0.16f;
    float fogMix = lerp(fogAqua, fogCoral, t);

    float3 col = lerp(base, 1.0.xxx, fogMix);

    // ★追加: プロシージャル和紙マテリアルを適用
    ApplyProceduralPaper(i.worldpos.xyz, col, n, 0.4, 0.8);

    // 立体感（控えめ）
    float up = saturate(n.y);
    up = smoothstep(0.2f, 0.9f, up);
    col *= lerp(0.97f, 1.03f, up);

    return float4(col, 1.0f);
}








