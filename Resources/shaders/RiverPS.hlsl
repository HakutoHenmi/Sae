// Resources/shaders/RiverPS.hlsl
Texture2D gTex : register(t0); 
SamplerState gSmp : register(s0);

cbuffer CBFrame : register(b0) { row_major float4x4 gView; row_major float4x4 gProj; row_major float4x4 gViewProj; float3 gCamPos; float gTime; };
cbuffer CBObj : register(b1) { row_major float4x4 gWorld; float4 gColor; };

struct PSIn { 
    float4 svpos : SV_POSITION; 
    float3 worldPos: TEXCOORD0; 
    float3 normal : TEXCOORD1; 
    float2 uv : TEXCOORD2; 
};

float4 main(PSIn i) : SV_TARGET { 
    float4 texColor = gTex.Sample(gSmp, i.uv);
    float4 finalColor = texColor;
    
    // 透過度などを加味する場合
    // finalColor.a = 0.8f; 
    
    return finalColor; 
}
