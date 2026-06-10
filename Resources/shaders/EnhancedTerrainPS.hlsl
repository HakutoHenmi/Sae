// EnhancedTerrainPS.hlsl
#include "EnhancedTerrain.hlsli"

// Texture Slots
Texture2D gSplatMap : register(t0); // RGBA weights for textures
Texture2D gLayer0   : register(t1); // e.g., Grass
Texture2D gLayer1   : register(t2); // e.g., Rock
Texture2D gLayer2   : register(t3); // e.g., Dirt
Texture2D gLayer3   : register(t4); // e.g., Sand
Texture2D gDetail   : register(t5); // Micro detail noise
Texture2D gShadowMap : register(t6);

SamplerState gSmp : register(s0);
SamplerComparisonState gShadowSmp : register(s1);

// Light structures
struct DirLight { float3 dir; float pad0; float3 color; float pad1; uint enabled; float3 pad2; };
struct PointLight { float3 pos; float pad0; float3 color; float range; float3 atten; float pad1; uint enabled; float3 pad2; };
struct SpotLight { float3 pos; float pad0; float3 dir; float range; float3 color; float inner; float3 atten; float outer; uint enabled; float3 pad2; };
struct AreaLight { float3 pos; float pad0; float3 color; float range; float3 right; float halfWidth; float3 up; float halfHeight; float3 dir; float pad1; float3 atten; float pad2; uint enabled; float3 pad3; };

#define MAX_DIR 1
#define MAX_POINT 4
#define MAX_SPOT 4
#define MAX_AREA 4

cbuffer CBLight : register(b2) {
    float3 gAmbientColor;
    float padA0;
    DirLight gDir[MAX_DIR];
    PointLight gPoint[MAX_POINT];
    SpotLight gSpot[MAX_SPOT];
    AreaLight gArea[MAX_AREA];
    row_major float4x4 gShadowMatrix;
};

// --- Utilities ---
float GetAttenuation(float3 atten, float d) { return 1.0 / (atten.x + atten.y * d + atten.z * d * d); }
float3 BlinnPhong(float3 L, float3 V, float3 N, float3 C, float3 A) {
    float NdotL = max(dot(N, L), 0.0); float3 diff = A * C * NdotL;
    float3 H = normalize(L + V); float NdotH = max(dot(N, H), 0.0);
    float3 spec = C * pow(NdotH, 64.0) * 0.2; return diff + spec;
}

float CalcShadow(float3 worldPos) {
    float4 shadowPos = mul(float4(worldPos, 1.0f), gShadowMatrix);
    float3 projCoords = shadowPos.xyz / shadowPos.w;
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;
    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f || projCoords.z < 0.0f || projCoords.z > 1.0f)
        return 1.0f;
    float shadow = 0.0f;
    float texelSize = 1.0f / 2048.0f;
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            shadow += gShadowMap.SampleCmpLevelZero(gShadowSmp, projCoords.xy + float2(x, y) * texelSize, projCoords.z).r;
        }
    }
    return shadow / 9.0f;
}

float4 main(VSOutput input) : SV_TARGET {
    float3 V = normalize(gCamPos - input.worldPos);
    float dist = length(gCamPos - input.worldPos);

    // 1. Splatting & Tiling Reduction
    // 遠くのタイリングを目立たなくするために、距離に応じてUVスケールを変えてブレンドする
    float2 uvNear = input.uv * 10.0f;
    float2 uvFar  = input.uv * 2.0f;
    float distWeight = saturate(dist / 50.0f);
    
    float4 splat = gSplatMap.Sample(gSmp, input.uv);
    
    // 各レイヤーのテクスチャをサンプリング
    float4 c0 = lerp(gLayer0.Sample(gSmp, uvNear), gLayer0.Sample(gSmp, uvFar), distWeight);
    float4 c1 = lerp(gLayer1.Sample(gSmp, uvNear), gLayer1.Sample(gSmp, uvFar), distWeight);
    float4 c2 = lerp(gLayer2.Sample(gSmp, uvNear), gLayer2.Sample(gSmp, uvFar), distWeight);
    float4 c3 = lerp(gLayer3.Sample(gSmp, uvNear), gLayer3.Sample(gSmp, uvFar), distWeight);

    // 2. Height Blending (高度なブレンディング)
    // テクスチャのアルファチャンネルを高さとして使用し、境界線をより複雑にする
    float h0 = c0.a + splat.r;
    float h1 = c1.a + splat.g;
    float h2 = c2.a + splat.b;
    float h3 = c3.a + splat.a;
    
    float depth = 0.1f;
    float maxH = max(h0, max(h1, max(h2, h3)));
    float w0 = max(0, h0 - maxH + depth);
    float w1 = max(0, h1 - maxH + depth);
    float w2 = max(0, h2 - maxH + depth);
    float w3 = max(0, h3 - maxH + depth);
    
    float totalW = w0 + w1 + w2 + w3;
    float4 albedo = (c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3) / totalW;

    // 3. Detail Texture
    float detail = gDetail.Sample(gSmp, input.uv * 50.0f).r;
    albedo.rgb *= lerp(0.8f, 1.2f, detail);

    // 4. Lighting
    float3 N = normalize(input.normal);
    float3 finalColor = albedo.rgb * gAmbientColor;
    float shadowFactor = CalcShadow(input.worldPos);

    for(int i=0; i<MAX_DIR; ++i) if(gDir[i].enabled) finalColor += BlinnPhong(normalize(-gDir[i].dir), V, N, gDir[i].color, albedo.rgb) * shadowFactor;
    for(int i=0; i<MAX_POINT; ++i) if(gPoint[i].enabled) { float3 Lv = gPoint[i].pos - input.worldPos; float d = length(Lv); if(d < gPoint[i].range) finalColor += BlinnPhong(normalize(Lv), V, N, gPoint[i].color, albedo.rgb) * GetAttenuation(gPoint[i].atten, d); }
    // (SpotLight/AreaLight also supported as per Renderer logic if needed)

    return float4(finalColor, input.color.a);
}
