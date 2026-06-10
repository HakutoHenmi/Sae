// Resources/Shaders/Distortion.hlsl
cbuffer CBFrame : register(b0) { row_major float4x4 gView; row_major float4x4 gProj; row_major float4x4 gViewProj; float3 gCamPos; float gTime; };
cbuffer CBObj : register(b1) { row_major float4x4 gWorld; float4 gColor; };

struct InstanceData {
    row_major float4x4 world;
    float4 color;
    float4 uvScaleOffset;
};
StructuredBuffer<InstanceData> gInstanceData : register(t2);

struct VSIn {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
    float3 nrm : NORMAL;
    float4 weights : WEIGHTS;
    uint4 indices : BONES;
};

struct VSOut {
    float4 svpos : SV_POSITION;
    float4 clipPos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : COLOR0;
};

VSOut main(VSIn v, uint instanceID : SV_InstanceID) {
    VSOut o;
    
    // インスタンスデータまたは定数バッファからワールド行列と色を取得
    float4x4 world = gWorld;
    float4 color = gColor;
    
    // インスタンスIDが有効な範囲ならインスタンスデータを使用 (t2のバインド有無を判定することはできないが、
    // ここではインスタンス描画時に SV_InstanceID が 0 以外になること、あるいは
    // Renderer側で適切にバインドされることを前提とする)
    // 実際には Renderer 側でインスタンス描画時のみこのシェーダーを適切にセットアップする。
    [branch]
    if (instanceID < 1024) { // 安全策としての最大数
        world = gInstanceData[instanceID].world;
        color = gInstanceData[instanceID].color;
    }

    float4 wp = mul(v.pos, world);
    float4 vp = mul(wp, gView);
    o.svpos = mul(vp, gProj);
    o.clipPos = o.svpos;
    o.uv = v.uv;
    o.color = color;
    return o;
}

Texture2D gBackdropTex : register(t0); // Current scene color
Texture2D gDistortionMap : register(t1); // Normal map for distortion
SamplerState gSmp : register(s0);

float4 ps_main(VSOut i) : SV_TARGET {
    // Convert clip space to screen UV
    float2 screenUV = i.clipPos.xy / i.clipPos.w;
    screenUV.x = screenUV.x * 0.5f + 0.5f;
    screenUV.y = -screenUV.y * 0.5f + 0.5f;

    // 中心(0.5, 0.5)からの距離
    float dist = length(i.uv - 0.5f);
    
    // i.color.a は 1.0(開始) から 0.0(終了) へ変化する
    float progress = 1.0f - i.color.a;

    // 波紋が外側に広がりながら消えていく自然な表現
    // ポリゴンの外枠がパツンと切れないようにエッジを柔らかくする
    float edgeMask = smoothstep(0.5f, 0.35f, dist);

    // 中心からドーナツ状にフェードアウトするマスク
    // progressが進むにつれて、内側(穴)が大きくなり、外側へと波紋の帯が移動していく
    float ringInner = progress * 0.4f; // 穴の広がる速度
    float ringOuter = ringInner + 0.15f; // 波紋の帯の太さ
    float waveMask = smoothstep(ringInner - 0.05f, ringInner, dist) * smoothstep(ringOuter, ringOuter - 0.1f, dist);

    // 法線マップから歪み方向を取得（rg成分がXYの歪み）
    float3 n = gDistortionMap.Sample(gSmp, i.uv).rgb * 2.0f - 1.0f;
    
    // 歪みの強度
    // i.color.a自体で全体的なフェードアウトもかけつつ、waveMaskでリング状にする
    float strength = i.color.a * 0.2f * edgeMask * waveMask;
    
    // 歪みオフセット
    float2 offset = n.xy * strength;

    // クランプしてバックドロップからサンプリング
    float2 distortedUV = clamp(screenUV + offset, 0.001f, 0.999f);
    float3 sceneColor = gBackdropTex.Sample(gSmp, distortedUV).rgb;
    
    // 色収差（Chromatic aberration）
    float aberration = 0.5f * strength; 
    [branch]
    if (aberration > 0.001f) {
        float3 colorR = gBackdropTex.Sample(gSmp, distortedUV + float2(offset.x * aberration, 0)).rgb;
        float3 colorB = gBackdropTex.Sample(gSmp, distortedUV - float2(offset.x * aberration, 0)).rgb;
        sceneColor.r = colorR.r;
        sceneColor.b = colorB.b;
    }

    return float4(sceneColor, 1.0f); // i.color.rgb は乗算せずシーンの色のまま返す
}
