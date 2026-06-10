#pragma pack_matrix(row_major)

struct InstanceData {
    matrix world;
    float4 color;
    float4 uvScaleOffset; // x:scaleU, y:scaleV, z:offsetU, w:offsetV
};

StructuredBuffer<InstanceData> gInstanceData : register(t2);

cbuffer ViewProjection : register(b0)
{
    matrix view;
    matrix projection;
    matrix viewProj;
    float3 cameraPos;
    float time;
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD, uint instanceID : SV_InstanceID)
{
    VSOutput output;
    InstanceData data = gInstanceData[instanceID];
    
    output.svpos = mul(pos, mul(data.world, mul(view, projection)));
    
    // Apply UV scale and offset from instance data
    output.uv.x = uv.x * data.uvScaleOffset.x + data.uvScaleOffset.z;
    output.uv.y = uv.y * data.uvScaleOffset.y + data.uvScaleOffset.w;
    
    output.color = data.color;
    return output;
}
