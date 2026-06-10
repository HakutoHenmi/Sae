// Resources/shaders/RichPostProcess.hlsl
// 和風プレミアム・セルシェーディング (深度バッファからの法線再構築+エッジ検出)
// 水墨汚れを排除し、完全クリーン版に変更（和紙表現はオブジェクト側でプロシージャルに行うため削除）。

Texture2D gScene : register(t0); 
Texture2D gPaper : register(t1);   // 未使用になったが互換性のため宣言は残す
Texture2D gInkWash : register(t2); // 未使用になったが互換性のため宣言は残す
Texture2D gDepth : register(t3);   // 深度バッファ
SamplerState gSmp : register(s0);

cbuffer CBPost : register(b0) { 
    float gTime; 
    float gNoiseStrength; 
    float gDistortion; 
    float gChromaShift; 
    float gVignette;    
    float gScanline;    
    float2 pad; 
};

static const float2 texelSize = float2(1.0 / 1920.0, 1.0 / 1080.0);

float luminance(float3 rgb) { return dot(rgb, float3(0.299, 0.587, 0.114)); }

// 深度バッファからワールド空間の法線を再構築
float3 ReconstructNormal(float2 uv) {
    float dc = gDepth.Sample(gSmp, uv).r;
    float dl = gDepth.Sample(gSmp, uv - float2(texelSize.x, 0)).r;
    float dr = gDepth.Sample(gSmp, uv + float2(texelSize.x, 0)).r;
    float dt = gDepth.Sample(gSmp, uv - float2(0, texelSize.y)).r;
    float db = gDepth.Sample(gSmp, uv + float2(0, texelSize.y)).r;
    
    float3 n;
    n.x = dl - dr;
    n.y = dt - db;
    n.z = 2.0 * texelSize.x;
    return normalize(n);
}

float4 main(float4 svpos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET {
    float2 centerOffset = uv - 0.5;
    
    // 和紙テクスチャ表現は削除。そのままのUVを使用。
    float2 distortedUV = uv;
    
    // Scene Sampling
    float3 sceneCol = gScene.Sample(gSmp, distortedUV).rgb;
    
    // -----------------------------------------------------------------
    // エッジ検出 (深度ベース + 法線再構築ベース)
    // -----------------------------------------------------------------
    float offset = 1.5;
    float2 offX = float2(texelSize.x * offset, 0);
    float2 offY = float2(0, texelSize.y * offset);

    float DC = gDepth.Sample(gSmp, distortedUV).r;
    float DL = gDepth.Sample(gSmp, distortedUV - offX).r;
    float DR = gDepth.Sample(gSmp, distortedUV + offX).r;
    float DT = gDepth.Sample(gSmp, distortedUV - offY).r;
    float DB = gDepth.Sample(gSmp, distortedUV + offY).r;

    float edgeL = DL - DC;
    float edgeR = DC - DR;
    float edgeB = DB - DC;
    float edgeT = DC - DT;
    float depthEdge = step(0.0002, max(abs(edgeL - edgeR), abs(edgeT - edgeB)));

    float3 NC = ReconstructNormal(distortedUV);
    float3 NL = ReconstructNormal(distortedUV - offX);
    float3 NR = ReconstructNormal(distortedUV + offX);
    float3 NT = ReconstructNormal(distortedUV - offY);
    float3 NB = ReconstructNormal(distortedUV + offY);
    
    float nEdge = min(min(dot(NL, NC), dot(NR, NC)), min(dot(NT, NC), dot(NB, NC)));
    float normalEdge = step(nEdge, 0.85);

    float edge = max(normalEdge, depthEdge);
    
    // -----------------------------------------------------------------
    // Edge Coloring & Integration
    // -----------------------------------------------------------------
    float lum = luminance(sceneCol);
    float edgePower = lerp(0.1, 0.4, lum);
    float3 finalEdgeColor = max(sceneCol * 0.4, saturate(sceneCol - edgePower)) * float3(0.8, 0.8, 1.0);
    
    edge *= 0.7; // 和紙の影響をなくす
    sceneCol = lerp(sceneCol, finalEdgeColor, edge * gChromaShift);

    // -----------------------------------------------------------------
    // Overall Aesthetic (Color Grading)
    // -----------------------------------------------------------------
    sceneCol = lerp(float3(lum, lum, lum), sceneCol, 0.65);
    sceneCol += float3(0.02, 0.02, 0.05) * (1.0 - lum);

    // 和紙オーバーレイ処理は削除

    // -----------------------------------------------------------------
    // Action Effects & Polish
    // -----------------------------------------------------------------
    if (gDistortion > 0.001) {
        float blurStrength = gDistortion * 0.15;
        float3 blurCol = 0;
        for (int s = 0; s < 5; ++s) {
            blurCol += gScene.Sample(gSmp, saturate(distortedUV - centerOffset * (float)s / 4.0 * blurStrength)).rgb;
        }
        sceneCol = lerp(sceneCol, blurCol / 5.0, 0.5);
    }

    // ダメージ/ピンチ時の赤いビネット
    if (gVignette > 0.001) {
        float distSq = dot(centerOffset, centerOffset);
        float damageVignette = saturate(distSq * 3.0f * gVignette);
        sceneCol = lerp(sceneCol, float3(1.0, 0.1, 0.1), damageVignette * 0.6f);
    }

    sceneCol *= 1.15;
    
    // ヒット演出 (ローヘルス等の持続的演出)
    float gray = luminance(sceneCol);
    sceneCol = lerp(sceneCol, float3(gray, gray, gray + 0.05) * 1.35, gScanline * 0.8);

    return float4(sceneCol, 1.0);
}
