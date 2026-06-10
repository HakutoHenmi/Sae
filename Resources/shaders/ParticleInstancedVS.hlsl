struct InstanceData { row_major float4x4 world; float4 color; float4 uvScaleOffset; };
StructuredBuffer<InstanceData> gInstanceData : register(t2);
cbuffer CBFrame : register(b0) { row_major float4x4 gView; row_major float4x4 gProj; row_major float4x4 gViewProj; float3 gCamPos; float gTime; };
struct VSOutput { float4 svpos : SV_POSITION; float2 uv : TEXCOORD; float4 color : COLOR; };
VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD, uint instanceID : SV_InstanceID) {
    VSOutput output;
    InstanceData data = gInstanceData[instanceID];
    output.svpos = mul(pos, mul(data.world, mul(gView, gProj)));
    output.uv.x = uv.x * data.uvScaleOffset.x + data.uvScaleOffset.z;
    output.uv.y = uv.y * data.uvScaleOffset.y + data.uvScaleOffset.w;
    output.color = data.color;
    return output;
}
