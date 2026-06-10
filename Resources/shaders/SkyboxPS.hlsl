#pragma pack_matrix(row_major)

#include "Space.hlsli"

TextureCube gCubeMap : register(t0);
SamplerState gSampler : register(s0);

cbuffer CBFrame : register(b0) {
    matrix gView;
    matrix gProj;
    matrix gViewProj;
    float3 gCameraPos;
    float  gTime;
    float4 gWindParams;
    float3 gPlayerPos;
    uint   gUseCubemapBackground;
};

struct PSIn {
    float4 svpos : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

float4 main(PSIn p) : SV_TARGET {
    float3 dir = normalize(p.texCoord);
    
    float3 color;
    if (gUseCubemapBackground != 0) {
        // Cubemapから景色をサンプリング
        color = gCubeMap.Sample(gSampler, dir).rgb;
    } else {
        // 超高速化＆軽量化された美しい宇宙空間の生成（TDRの原因を取り除いた完全版）
        color = GetProceduralSpaceColor(dir, gTime);
    }

    return float4(color, 1.0);
}
