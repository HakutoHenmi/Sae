// EnhancedTerrain.hlsli
#pragma pack_matrix(row_major)

cbuffer CBFrame : register(b0) {
    row_major float4x4 gView;
    row_major float4x4 gProj;
    row_major float4x4 gViewProj;
    float3 gCamPos;
    float gTime;
};

struct InstanceData {
    row_major float4x4 world;
    float4 color;
};

StructuredBuffer<InstanceData> gInstances : register(t2);

struct VSInput {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float4 weights : WEIGHTS;
    uint4 indices : BONES;
    uint instanceID : SV_InstanceID;
};

struct VSOutput {
    float4 svpos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 color : COLOR;
};
