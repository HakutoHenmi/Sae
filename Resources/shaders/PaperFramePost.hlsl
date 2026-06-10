// Resources/shaders/PaperFramePost.hlsl
// 和紙フレーム・ポストプロセス（画面全体に和紙質感をオーバーレイし、端にかけてシームレスに和紙の地色と水墨フレームに溶け込ませる）
// 【完全クリーン仕様】中間の黒い水墨汚れを100%完全に排除し、中央を極めてクリアに保ちつつ、
// 秒間12コマ（12fps）の大振りのダイナミック・ジッターによって、手書きのアナログ感を最高潮に高めます。

Texture2D gScene : register(t0); 
Texture2D gPaper : register(t1);   // 和紙テクスチャ
Texture2D gFrame : register(t2);   // フレームマスク（vignette.png: 中央が白1.0、端が黒0.0の墨ビネット）
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

float4 main(float4 svpos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET {
    // 1. 3Dシーンカラーの取得
    float3 sceneCol = gScene.Sample(gSmp, uv).rgb;

    // 2. ゆっくり流れる連続的なUVスクロール
    // パラパラとしたジッターではなく、時間経過で斜め方向にゆっくりと和紙が流れるようにします
    // 速度を少しだけ早めました
    float2 scrollOffset = float2(-0.030f, 0.020f) * gTime;

    // 3. アスペクト比補正とスクロールを加えた紙テクスチャのサンプリング
    float aspect = 1920.0 / 1080.0;
    float2 uvPaper = uv;
    uvPaper.x *= aspect;
    uvPaper *= 2.5; // タイリングを戻して自然な繊維感へ
    uvPaper += scrollOffset;

    float3 paperCol = gPaper.Sample(gSmp, uvPaper).rgb;

    // 4. 3Dシーンへの「和紙質感オーバーレイ(Overlay)」処理
    float3 overlayCol;
    for(int i = 0; i < 3; ++i) {
        overlayCol[i] = (sceneCol[i] < 0.5f) 
            ? (2.0f * sceneCol[i] * paperCol[i]) 
            : (1.0f - 2.0f * (1.0f - sceneCol[i]) * (1.0f - paperCol[i]));
    }
    // 繊維の主張を抑え、3Dシーンをクリアに見せる
    float3 paperScene = lerp(sceneCol, overlayCol, saturate(gNoiseStrength * 0.45f));

    // 5. 【水墨汚れの完全排除】ビネットマスクの計算
    // vignette.png の黒いしぶき（frameCol）を乗算するのを100%完全にカットし、
    // 単に「周辺に向けて和紙に溶け込むためのマスク」としてのみ使用します。
    float3 frameCol = gFrame.Sample(gSmp, uv).rgb;
    float mask = frameCol.r;

    // 四隅が露出するのを防ぐため、円形ビネットをブレンドアルファに加算合成します。
    float2 centerDist = uv - 0.5f;
    float circleMask = saturate(dot(centerDist, centerDist) * (1.0f + gVignette * 4.0f));

    // vignetteのしぶき形状の美しさを活かしつつ、四隅を含む周辺を確実に美しい和紙で覆います。
    float blendAlpha = saturate((1.0f - mask) + circleMask);

    // 黒いインク（frameCol）の乗算を完全にやめ、クリアな和紙（paperCol）とシーン（paperScene）をブレンド！
    float3 finalCol = lerp(paperScene, paperCol, blendAlpha);

    // ★追加: ダメージを受けた瞬間の赤いダメージビネット
    if (gDistortion > 0.001) {
        float distSq = dot(centerDist, centerDist);
        float damageVignette = saturate(distSq * 3.0f * gDistortion);
        finalCol = lerp(finalCol, float3(1.0, 0.1, 0.1), damageVignette * 0.6f);
    }

    return float4(finalCol, 1.0f);
}
