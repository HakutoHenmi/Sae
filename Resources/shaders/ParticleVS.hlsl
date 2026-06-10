#pragma pack_matrix(row_major)

// b0: Frame Shared Data
cbuffer ViewProjection : register(b0)
{
    matrix view;
    matrix projection;
    matrix viewProj;
    float3 cameraPos;
    float time;
};

// b1: Particle Instance Data
cbuffer ParticleTransform : register(b1)
{
    matrix world;
    float4 color;
    float4 uvScaleOffset; // x:scaleU, y:scaleV, z:offsetU, w:offsetV
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD) {
    VSOutput output;
    output.svpos = mul(pos, mul(world, mul(view, projection)));
    
    // Apply UV scale and offset
    output.uv.x = uv.x * uvScaleOffset.x + uvScaleOffset.z;
    output.uv.y = uv.y * uvScaleOffset.y + uvScaleOffset.w;
    
    output.color = color;
    return output;
}
