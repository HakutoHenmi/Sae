cbuffer CBFrame : register(b0) {
    row_major float4x4 gView;
    row_major float4x4 gProj;
    row_major float4x4 gViewProj;
    float3 gCamPos;
    float gTime;
};

struct VSOutput {
    float4 svpos : SV_POSITION;
    float2 uv    : TEXCOORD;
    float4 color : COLOR;
};

// --- ノイズ関数群 ---
float hash(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// FBM (Fractal Brownian Motion) - 軽量版 (2オクターブ)
float fbm(float2 p) {
    float val = 0.5 * noise(p);
    p = float2(p.x * 0.866 - p.y * 0.5, p.x * 0.5 + p.y * 0.866) * 2.17;
    val += 0.235 * noise(p);
    return val;
}

float4 main(VSOutput input) : SV_TARGET {
    // カメラ近接フェード (近い場合は重いノイズ計算をスキップ)
    float camFade = smoothstep(3.0, 10.0, input.svpos.w);
    if (camFade <= 0.01) discard;

    float2 uv = input.uv;
    float2 center = uv - 0.5;
    float baseDist = length(center);

    // 画面外周は無条件で破棄 (最適化)
    if (baseDist >= 0.5) discard;

    // 時間によるスクロール (煙特有の湧き上がる動き)
    float2 uvScroll = uv + float2(0.0, -gTime * 0.2);

    // ノイズで歪み(Distortion)を加える
    float noise1 = fbm(uvScroll * 5.0 + float2(gTime * 0.1, 0.0));
    float noise2 = fbm(uvScroll * 10.0 - float2(0.0, gTime * 0.15));
    float combinedNoise = (noise1 * 0.6 + noise2 * 0.4);

    // 歪ませた中心からの距離を再計算
    float dist = length(center + (combinedNoise - 0.5) * 0.2);

    // 境界のぼかし (ソフトパーティクル風)
    float alpha = smoothstep(0.4, 0.1, dist);

    // ノイズの強さに応じて煙の「濃淡（ムラ）」を作る
    alpha *= smoothstep(0.2, 0.8, combinedNoise);

    // 最終的なフェード
    alpha *= camFade;
    
    // パーティクルの頂点カラーとアルファを掛け合わせる
    float4 finalColor = input.color;
    finalColor.a *= alpha;

    if (finalColor.a <= 0.01f) discard;

    return finalColor;
}
