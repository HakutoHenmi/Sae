cbuffer CBFrame : register(b0) { row_major float4x4 gView; row_major float4x4 gProj; row_major float4x4 gViewProj; float3 gCamPos; float gTime; };
cbuffer CBObj : register(b1) { row_major float4x4 gWorld; float4 gColor; };

struct VSIn { 
	float4 pos : POSITION; 
	float2 uv : TEXCOORD0; 
	float3 nrm : NORMAL; 
	float4 weights : WEIGHTS; 
	uint4 indices : BONES; 
};
struct VSOut { float4 svpos : SV_POSITION; float3 worldPos: TEXCOORD0; float3 normal : TEXCOORD1; float2 uv : TEXCOORD2; };

VSOut main(VSIn v) { 
    VSOut o; 
    float4 wp = mul(v.pos, gWorld); 
    o.worldPos = wp.xyz; 
    float3 wn = mul(float4(v.nrm, 0), gWorld).xyz; 
    o.normal = normalize(wn); 
    float4 vp = mul(wp, gView); 
    o.svpos = mul(vp, gProj); 
    o.uv = v.uv; 
    return o; 
}
