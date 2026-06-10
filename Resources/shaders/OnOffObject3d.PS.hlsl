#include "Obj.hlsli"

Texture2D tex : register(t0);
SamplerState smp : register(s0);

// 枠の太さ（UV基準） 0.01〜0.05で調整
static const float kLine = 0.02f;

float4 main(VSOutput i) : SV_TARGET
{
    // alpha < 0.5 を「枠だけモード」にする（C++側で 0.15 など）
    bool outlineMode = (color.a < 0.5f);

    // -----------------------------
    // 通常（スイッチ＆実体ブロック）：赤/青ベタ（透過なし）
    // ※虹を消したいので tex.Sample は使わない
    // -----------------------------
    if (!outlineMode)
    {
        return float4(color.rgb, 1.0f);
    }

    // -----------------------------
    // 枠だけ：UVの端だけ描く（中身は完全に消す）
    // -----------------------------
    float2 uv = i.uv;

    // uvが0..1じゃないモデルでも最低限動くようにする（面がタイルでも端線が出すぎない）
    // ※もしおかしくなるなら、この frac を消して そのまま uv を使ってOK
    uv = frac(uv);

    float2 d = min(uv, 1.0f - uv); // 端までの距離
    float dist = min(d.x, d.y);

    // アンチエイリアス（線がギザらない）
    float aa = fwidth(dist);

    // dist < kLine のところが線
    float w = 1.0f - smoothstep(kLine - aa, kLine + aa, dist);

    // 中身を捨てる（これで“面”が消える）
    clip(w - 0.01f);

    // 線色（灰色固定）
    return float4(0.65f, 0.65f, 0.65f, 1.0f);
}
