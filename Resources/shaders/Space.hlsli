// Extremely lightweight, high-performance procedural space environment

// Simple hash for stars (1 calculation, extremely fast)
float hashFast(float3 p) {
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Pseudo-3D noise based on trigonometric waves for Nebulas
// This avoids expensive 3D value noise and FBM loops, preventing GPU TDR!
float waveNoise(float3 p) {
    float n = 0.0;
    n += sin(p.x * 2.1 + p.y * 1.3 - p.z * 0.8);
    n += cos(p.y * 3.2 + p.z * 1.7 + p.x * 0.5) * 0.7;
    n += sin(p.z * 4.5 + p.x * 2.6 + p.y * 1.2) * 0.4;
    return n * 0.47; // Normalize roughly to -1.0 ~ 1.0
}

float3 GetProceduralSpaceColor(float3 dir, float time) {
    // 1. Deep Space Background
    float3 color = float3(0.01, 0.015, 0.025) * 0.5;

    // 2. High-performance Nebula (Using sine waves instead of heavy FBM)
    float3 nebDir = dir * 1.5;
    float w1 = waveNoise(nebDir + float3(time * 0.02, 0, 0));
    float w2 = waveNoise(nebDir * 1.8 - float3(0, time * 0.015, 0));
    
    // Smooth and map sine waves to beautiful nebula colors
    float n1 = smoothstep(0.2, 0.9, w1 * 0.5 + 0.5);
    float n2 = smoothstep(0.3, 1.0, w2 * 0.5 + 0.5);

    float3 nebColor1 = float3(0.15, 0.35, 0.7) * n1;   // Blue nebula
    float3 nebColor2 = float3(0.6, 0.15, 0.45) * n2;   // Purple/Pink nebula
    color += nebColor1;
    color += nebColor2;

    // Milky Way Band
    float mw = smoothstep(0.5, 0.9, waveNoise(dir * 2.5 + float3(n1, n2, 0)) * 0.5 + 0.5);
    color += float3(0.1, 0.2, 0.3) * mw * 0.8;

    // 3. Ultra-fast Stars (No 3D Lerps!)
    // We use a high-frequency grid trick to avoid banding and make sharp stars
    // Added length() check to make stars round instead of blocky
    float3 gridPos1 = dir * 250.0;
    float3 starGrid1 = floor(gridPos1);
    float3 starFrac1 = frac(gridPos1) - 0.5;
    float s1 = hashFast(starGrid1);
    if (s1 > 0.99) { // 1% chance for a star in this grid cell
        float intensity = smoothstep(0.4, 0.0, length(starFrac1)); // Make it a round dot
        float twinkle = sin(time * 3.0 + s1 * 10.0) * 0.5 + 0.5;
        color += float3(0.9, 0.9, 1.0) * intensity * ((s1 - 0.99) * 100.0) * twinkle;
    }

    float3 gridPos2 = dir * 120.0 + float3(12.3, 45.6, 78.9);
    float3 starGrid2 = floor(gridPos2);
    float3 starFrac2 = frac(gridPos2) - 0.5;
    float s2 = hashFast(starGrid2);
    if (s2 > 0.992) {
        float intensity = smoothstep(0.4, 0.05, length(starFrac2));
        float twinkle = sin(time * 2.0 + s2 * 10.0) * 0.4 + 0.6;
        float3 starColor = lerp(float3(0.6, 0.8, 1.0), float3(1.0, 0.8, 0.6), hashFast(starGrid2 * 1.5));
        color += starColor * intensity * ((s2 - 0.992) * 125.0) * twinkle;
    }

    float3 gridPos3 = dir * 60.0 - float3(9.8, 7.6, 5.4);
    float3 starGrid3 = floor(gridPos3);
    float3 starFrac3 = frac(gridPos3) - 0.5;
    float s3 = hashFast(starGrid3);
    if (s3 > 0.994) {
        float intensity = smoothstep(0.4, 0.02, length(starFrac3));
        float twinkle = sin(time * 1.2 + s3 * 10.0) * 0.2 + 0.8;
        color += float3(1.0, 0.95, 0.9) * intensity * ((s3 - 0.994) * 160.0) * twinkle;
    }

    // Gentle HDR tone mapping
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));

    return color;
}

// ★反射用の軽量環境色取得関数 (リアルな宇宙空間)
// waveNoise呼出は2回までに抑えてTDR回避しつつ、リアルな見た目を実現。
float3 GetReflectionEnvColor(float3 dir, float time) {
    // 1. Deep Space Background (深い宇宙色)
    float3 color = float3(0.005, 0.008, 0.02);

    // 2. ネビュラ層1: 青〜紫の雲 (waveNoise 1回目)
    float3 nebDir1 = dir * 2.0;
    float w1 = waveNoise(nebDir1 + float3(time * 0.015, time * 0.01, 0));
    float n1 = smoothstep(0.25, 0.85, w1 * 0.5 + 0.5);
    float3 nebColor1 = lerp(float3(0.08, 0.15, 0.45), float3(0.25, 0.1, 0.4), n1) * n1;
    color += nebColor1 * 0.8;

    // 3. ネビュラ層2: 暖色系のアクセント (waveNoise 2回目)
    float3 nebDir2 = dir * 3.0 + float3(7.3, 2.1, 5.7);
    float w2 = waveNoise(nebDir2 + float3(0, time * 0.008, time * 0.012));
    float n2 = smoothstep(0.4, 0.95, w2 * 0.5 + 0.5);
    float3 nebColor2 = lerp(float3(0.35, 0.12, 0.08), float3(0.5, 0.25, 0.15), n2) * n2;
    color += nebColor2 * 0.4;

    // 4. 銀河のバンド (天の川のような帯状の明るさ)
    float galaxyBand = exp(-8.0 * dir.y * dir.y); // Y=0 付近が最も明るい帯
    float galaxyNoise = frac(sin(dot(dir.xz * 50.0, float2(12.9898, 78.233))) * 43758.5453);
    color += float3(0.12, 0.10, 0.18) * galaxyBand * (0.5 + 0.5 * galaxyNoise);

    // 5. 方向ベースのグラデーション
    float upFactor = saturate(dir.y * 0.5 + 0.5);
    color += float3(0.02, 0.04, 0.08) * upFactor; // 上方向：わずかに青白く
    float downFactor = saturate(-dir.y * 0.5 + 0.3);
    color += float3(0.04, 0.02, 0.01) * downFactor; // 下方向：わずかに暖色

    // 6. 明るい疑似星 (ハッシュベース、ループなしで軽量)
    float3 gridPos = dir * 80.0;
    float3 starDir = floor(gridPos);
    float3 starFrac = frac(gridPos) - 0.5;
    float starHash = frac(sin(dot(starDir, float3(127.1, 311.7, 74.7))) * 43758.5453);
    
    // length() を使って丸い点にする
    float starIntensity = smoothstep(0.4, 0.05, length(starFrac));
    float starBright = step(0.985, starHash) * starHash * 2.0 * starIntensity;
    color += float3(0.9, 0.95, 1.0) * starBright;

    // HDR tone mapping (Reinhard)
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));

    return color;
}
