#include "Obj.hlsli"

static float Hash11(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

static float Noise21(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = Hash11(dot(i, float2(127.1, 311.7)));
    float b = Hash11(dot(i + float2(1, 0), float2(127.1, 311.7)));
    float c = Hash11(dot(i + float2(0, 1), float2(127.1, 311.7)));
    float d = Hash11(dot(i + float2(1, 1), float2(127.1, 311.7)));
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float4 main(VSOutput i) : SV_TARGET
{
    float3 n = normalize(i.normal);
    float3 v = normalize(cameraPos.xyz - i.worldpos.xyz);

    // 発光色：Inspectorでシアン/青寄りにすると「加速」感が出やすい
    float3 energyCol = color.rgb;

    // 角度で光る（縁が強くなる）
    float rim = 1.0 - saturate(dot(n, v));
    rim = pow(rim, 3.0);

    float2 uv = i.uv;

    // -----------------------------
    // 1) ベースの周方向フロー（リングに平行）
    // -----------------------------
    float flowDir = 1.0; // 逆なら -1.0
    float speed = 2.6; // 加速感=速め
    float bands = 26.0; // 流れの細かさ

    float u = uv.x * bands + time * speed * flowDir;

    float stripe = abs(frac(u) - 0.5) * 2.0;
    stripe = 1.0 - stripe;
    stripe = smoothstep(0.72, 1.0, stripe); // 細め

    // -----------------------------
    // 2) “矢印っぽい”チップ（方向感を出す）
    //    uv.x方向に並べ、timeで流す。uv.yで先端を作る
    // -----------------------------
    float chips = 10.0;
    float cell = uv.x * chips + time * (speed * 0.55) * flowDir;
    float id = floor(cell);
    float x = frac(cell); // 0..1

    // 各チップの強度（ランダムで点灯/消灯）
    float on = step(0.55, Hash11(id * 19.0 + 3.0));

    // 三角っぽい形（先端）: xが前に行くほど細くなる
    float tip = (1.0 - x);
    tip = tip * tip; // 先端強調

    // 幅（uv.yで中央寄りほど強く…モデルによって逆の場合あり）
    float y = abs(uv.y - 0.5) * 2.0; // 0中心
    float chipWidth = smoothstep(0.9, 0.2, y); // 中央強い

    float arrow = on * tip * chipWidth;
    arrow = smoothstep(0.2, 0.9, arrow);

    // -----------------------------
    // 3) 内側エッジを強く光らせる（ゲート感）
    //    uv.y を「内外」っぽく見立てて強調（合わなければ y を反転）
    // -----------------------------
    float inner = smoothstep(0.55, 0.98, uv.y); // 内側寄りが強い想定
    // 逆だったらこれに変える： float inner = smoothstep(0.55, 0.98, 1.0 - uv.y);

    // -----------------------------
    // 4) ゆらぎ + 脈動（エネルギー感）
    // -----------------------------
    float n1 = Noise21(uv * 8.0 + float2(time * 0.25, time * 0.15));
    float wobble = lerp(0.8, 1.2, n1);

    float pulse = 0.8 + 0.2 * sin(time * 3.5);

    // -----------------------------
    // 5) 合成：流れ + 矢印 + 内側光 + リム
    // -----------------------------
    float glow = 0.18;
    glow += rim * 1.0;
    glow += stripe * 1.3 * wobble;
    glow += arrow * 1.6; // 方向感の本体
    glow += inner * 0.9; // ゲート感
    glow *= (0.9 + pulse * 0.35);

    // 強めに出して“発光体”っぽく
    float3 rgb = energyCol * glow * 4.5;

    return float4(rgb, 1.0);
}
