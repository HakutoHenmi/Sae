#pragma pack_matrix(row_major)

Texture2D<float4> gDepthTex : register(t0);
Texture2D<float4> gSceneTex : register(t1);
SamplerState gSampler : register(s0);
SamplerState gLinearSampler : register(s1);

cbuffer CompositeConstants : register(b0) {
    matrix view;
    matrix projection;
    matrix viewProj;
    matrix invProjection;
    matrix invView;
    float3 cameraPos;
    float time;
    float3 corePosition;
    float isLiquidated;
    float3 blobColor;
    float padColor;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float3 GetViewPos(float2 uv, float viewZ) {
    float2 clipXY = uv * 2.0f - 1.0f;
    clipXY.y = -clipXY.y;
    float4 viewRay = mul(float4(clipXY, 1.0f, 1.0f), invProjection);
    viewRay.xyz /= viewRay.w;
    return viewRay.xyz * (viewZ / viewRay.z);
}

float4 main(VSOutput input) : SV_TARGET {
    float w, h;
    gDepthTex.GetDimensions(w, h);
    float2 texelSize = float2(1.0f / w, 1.0f / h);

    float centerZ = gDepthTex.SampleLevel(gSampler, input.uv, 0).r;
    if (centerZ >= 9999.0f) discard;

    // ナローバンド深度ブラー（SSFR）
    float sumZ = 0.0f;
    float sumW = 0.0f;
    const int R = 7;
    for (int y = -R; y <= R; y++) {
        for (int x = -R; x <= R; x++) {
            float2 off = float2((float)x, (float)y) * texelSize * 1.2f;
            float z = gDepthTex.SampleLevel(gSampler, input.uv + off, 0).r;
            if (z < 9999.0f) {
                float gw = exp(-(x*x + y*y) / (2.0f * 12.0f));
                sumZ += z * gw;
                sumW += gw;
            }
        }
    }
    float viewZ = sumW > 0.0f ? sumZ / sumW : centerZ;

    float3 viewPos = GetViewPos(input.uv, viewZ);
    float3 ddxP = ddx(viewPos);
    float3 ddyP = ddy(viewPos);
    float3 viewNormal = cross(ddxP, ddyP);
    float nLen = length(viewNormal);
    if (nLen < 0.00001f) viewNormal = float3(0, 0, -1);
    else viewNormal /= nLen;
    viewNormal.z = min(viewNormal.z, -0.02f);
    viewNormal = normalize(viewNormal);

    float4 worldPos4 = mul(float4(viewPos, 1.0f), invView);
    float3 worldPos = worldPos4.xyz;

    float3 viewDir = normalize(-viewPos);
    float3 lightDir = normalize(float3(0.4f, 0.6f, -1.0f));
    float NdotL = max(dot(viewNormal, lightDir), 0.0f);
    float NdotV = max(dot(viewNormal, viewDir), 0.0f);
    float fresnel = pow(1.0f - NdotV, 2.5f);

    // 屈折（背景シーンを法線でずらしてサンプル）
    // 透明な水風船のように歪みを大きくする
    float2 refractOffset = viewNormal.xy * (0.04f + fresnel * 0.02f);
    float3 refracted = gSceneTex.SampleLevel(gLinearSampler, input.uv + refractOffset, 0).rgb;

    // スライムの厚み（中心ほど厚いと仮定）
    float distCore = length(worldPos - corePosition);
    float thickness = smoothstep(1.5f, 0.0f, distCore); 

    // スライムの色（厚みがある中心ほど色が濃く・少し暗くなり、縁は透き通って明るい）
    float3 jellyColor = blobColor * (0.8f - thickness * 0.3f);
    
    // 光沢（濡れたゼリー特有の鋭いハイライト）
    float3 halfVec = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewNormal, halfVec), 0.0f), 400.0f); // より鋭く

    // 合成: 屈折した背景に色を乗せつつ、ベース色も加算
    float3 finalColor = refracted * lerp(float3(1.0f, 1.0f, 1.0f), jellyColor, 0.5f);
    finalColor += jellyColor * 0.6f;

    // リムライト・フレネル（縁が環境光を反射して白っぽく光る）
    float3 rimColor = float3(0.8f, 1.0f, 0.9f);
    finalColor += rimColor * fresnel * 0.8f;

    // ハイライト加算
    finalColor += float3(1.0f, 1.0f, 1.0f) * spec * 2.0f;

    // アルファ: 全体的に透けさせる（背景とアルファブレンドする）
    // 中央は薄く（0.4）、縁やハイライトは不透明に近づける
    float alpha = saturate(0.4f + fresnel * 0.4f + spec);
    
    return float4(finalColor, alpha);
}
