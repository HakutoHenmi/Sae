// ================================================
// UI SDF Pixel Shader（DirectX 12 / D3DCompile対象）
// 四角・円・三日月をピクセルシェーダだけで描く
// 画面空間(px)で処理 / グロー付 / 加算ブレンド推奨
// ================================================

cbuffer CBUI : register(b0)
{
    float2  uCenterPx;     // 画面上の中心位置(px)
    float2  uSizePx;       // 図形サイズ(px) … 四角は幅高、円は半径を uSizePx.x に入れる
    float2  uViewportPx;   // 画面解像度(px)（ViewportのWidth/Height）
    float   uLineWidth;    // 線の太さ(px)
    float   uGlow;         // グローの厚み(px)（外側のにじみ範囲）
    float4  uColor;        // ベース色（線色）
    int     uShape;        // 0:Square 1:Circle 2:Crescent
    float   uRound;        // 角丸半径(px)（四角用）
    float   uInner;        // 三日月の内側半径（円との差分用）
    float   uRotateRad;    // 回転（ラジアン） 2D図形を回転したい時
}

// 受け取り：頂点で出したUV（0-1）。全画面クアッドならそのまま使える
struct PSIn {
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;  // (0,1)範囲を想定
};

static float2 Rotate(float2 p, float a) {
    float c = cos(a), s = sin(a);
    return float2(c*p.x - s*p.y, s*p.x + c*p.y);
}

// 距離関数 -------------------------------
// 角丸矩形（中心原点、size=半サイズ、round=角丸半径）
float sdRoundedBox(float2 p, float2 halfSize, float roundR) {
    float2 q = abs(p) - halfSize + roundR;
    float2 m = max(q, 0.0);
    return length(m) - roundR + min(max(q.x, q.y), 0.0);
}

// 円（中心原点、半径 r）
float sdCircle(float2 p, float r) {
    return length(p) - r;
}

// 三日月：外円 r と 内円 rInner の差（外円∩内円の外側）
// dOuter==0 が外円の線、 dInner==0 が内円の線。
// “ライン”としては |dOuter| の近傍を主に使い、内円側でマスク。
float sdCrescent(float2 p, float rOuter, float rInner) {
    // 外円＋内円のCSG差　※ここでは “外円の距離” を返して、後段で内円で消す
    return sdCircle(p, rOuter);
}

// 共通：線＆グローの着色
//  d     : 距離（0=図形境界）
//  width : 線の半分厚み（px）に近いイメージ → uLineWidthをそのまま使用
//  glow  : グロー厚（px）
//  col   : ベース色
float4 drawLineGlow(float d, float width, float glow, float4 col, float innerMask)
{
    // 線のα（-width..+width を1→0へ）
    float lineAlpha = 1.0 - smoothstep(width-1.0, width+1.0, abs(d)); // 線の内側を強めに
    // グロー（線の外側からglow範囲で0→1→0のフェード）
    float glowAlpha = 1.0 - smoothstep(width, width + glow, abs(d));

    // ちょい内側を強調（発光コア）
    float core = 1.0 - smoothstep(0.0, width*0.6, abs(d));
    float4 glowCol = col * (0.55 * glowAlpha + 0.45 * core);

    // 内側マスク（三日月など内側を切るため）
    glowCol.a *= innerMask;

    return glowCol; // 加算ブレンド前提
}

// Crescent用の内側マスク（内円より内側は消す）
float innerMaskByInnerCircle(float2 p, float rInner) {
    // rInner 未使用時は常に1.0
    if (rInner <= 0.0) return 1.0;
    float din = sdCircle(p, rInner);
    // 内円の内側(din<0) → マスク0、外側→1
    return step(0.0, din);
}

float4 mainPS(PSIn i) : SV_TARGET
{
    // 画面UV→px座標へ（左上(0,0)）
    float2 uv = i.uv;
    float2 fragPx = uv * uViewportPx;

    // 図形中心基準へ
    float2 p = fragPx - uCenterPx;
    // 回転
    p = Rotate(p, uRotateRad);

    // 距離dの計算
    float d = 1e6;
    float innerMask = 1.0;

    if (uShape == 0) {
        // 角丸矩形：uSizePx=幅高、halfに
        d = sdRoundedBox(p, uSizePx * 0.5, uRound);
    } else if (uShape == 1) {
        // 円：uSizePx.x を半径として使用
        d = sdCircle(p, uSizePx.x);
    } else { // 2: Crescent
        d = sdCrescent(p, uSizePx.x, uInner);
        innerMask = innerMaskByInnerCircle(p, uInner);
    }

    return drawLineGlow(d, uLineWidth, uGlow, uColor, innerMask);
}