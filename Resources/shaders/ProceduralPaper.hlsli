// Resources/shaders/ProceduralPaper.hlsli
#ifndef PROCEDURAL_PAPER_HLSLI
#define PROCEDURAL_PAPER_HLSLI

// 3D Hash
float hash(float3 p) {
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3D Value Noise
float noise(float3 x) {
    float3 i = floor(x);
    float3 f = frac(x);
    // 滑らかな補間 (Quintic)
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    
    return lerp(lerp(lerp(hash(i + float3(0,0,0)), hash(i + float3(1,0,0)), f.x),
                     lerp(hash(i + float3(0,1,0)), hash(i + float3(1,1,0)), f.x), f.y),
                lerp(lerp(hash(i + float3(0,0,1)), hash(i + float3(1,0,1)), f.x),
                     lerp(hash(i + float3(0,1,1)), hash(i + float3(1,1,1)), f.x), f.y), f.z);
}

// FBM (Fractal Brownian Motion)
float fbm(float3 p) {
    float f = 0.0;
    float w = 0.5;
    for (int i = 0; i < 4; i++) {
        f += w * noise(p);
        p *= 2.0;
        w *= 0.5;
    }
    return f;
}

// プロシージャル和紙マテリアル適用関数
void ApplyProceduralPaper(float3 worldPos, inout float3 albedo, inout float3 normal, float scale = 1.0, float strength = 1.0) {
    
    // スケールを調整（細かすぎないようにする）
    float paperScale = 4.0 * scale;
    
    // 1. 和紙のベースとなる大きなムラ (ふんわりとした雲のようなムラ)
    float baseNoise = fbm(worldPos * paperScale);
    
    // 2. 和紙の細かいザラザラ（繊維のちり）
    // 倍率を8.0から4.0に下げて、テレビの砂嵐のような細かすぎるノイズを防ぐ
    float detailScale = paperScale * 4.0;
    float detailNoise = fbm(worldPos * detailScale);
    
    // コントラストを少し柔らかく調整
    float mura = smoothstep(0.3, 0.7, baseNoise);
    float zara = smoothstep(0.4, 0.7, detailNoise);
    
    // 3. 法線の乱れ (バンプマッピング)
    float epsilon = 0.01;
    // ザラザラ感だけを法線に適用 (偏微分)
    float nx = fbm((worldPos + float3(epsilon, 0, 0)) * detailScale) - detailNoise;
    float ny = fbm((worldPos + float3(0, epsilon, 0)) * detailScale) - detailNoise;
    float nz = fbm((worldPos + float3(0, 0, epsilon)) * detailScale) - detailNoise;

    // バンプの感度を大幅に下げる (5.0 -> 0.5)
    // ノイズの差分が大きすぎると元の法線を完全に上書きしてしまい、アルミホイルのように乱反射するため。
    float3 bumpNormal = float3(nx, ny, nz) * 0.5; 
    
    // バンプの強度をかける
    normal = normalize(normal + bumpNormal * strength);
    
    // 4. 色のブレンド
    
    // ムラによるごくわずかな明暗（濁りを防ぐためさらに控えめに）
    albedo *= lerp(0.9, 1.05, mura); 
    
    // 細かい繊維（白）を上からブレンドする
    float3 fiberColor = float3(1.0, 0.98, 0.95);
    
    // ザラザラノイズの白飛びを抑える (0.4 -> 0.15)
    albedo = lerp(albedo, fiberColor, zara * 0.15 * strength);
}
#endif // PROCEDURAL_PAPER_HLSLI
