cbuffer CBFrame : register(b0) { row_major float4x4 gView; row_major float4x4 gProj; row_major float4x4 gViewProj; float3 gCamPos; float gTime; };
cbuffer CBObj : register(b1) { row_major float4x4 gWorld; float4 gColor; };
cbuffer CBBone : register(b3) { row_major float4x4 gBones[128]; };

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
    
    // スキニング行列の合成
    float4x4 skinMat = 
        gBones[v.indices.x] * v.weights.x +
        gBones[v.indices.y] * v.weights.y +
        gBones[v.indices.z] * v.weights.z +
        gBones[v.indices.w] * v.weights.w;

    float4 localPos = v.pos;
    float4 skinnedPos = mul(localPos, skinMat);
    float4 localNrm = float4(v.nrm, 0.0f);
    float3 skinnedNrm = mul(localNrm, skinMat).xyz;

    float4 wp = mul(skinnedPos, gWorld); 
    o.worldPos = wp.xyz; 
    float3 wn = mul(float4(skinnedNrm, 0), gWorld).xyz; 
    o.normal = normalize(wn); 
    
    float4 vp = mul(wp, gView); 
    o.svpos = mul(vp, gProj); 
    o.uv = v.uv; 
    return o; 
}
