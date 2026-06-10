// Resources/shaders/RiverVS.hlsl
cbuffer CBFrame : register(b0) { row_major float4x4 gView; row_major float4x4 gProj; row_major float4x4 gViewProj; float3 gCamPos; float gTime; };
cbuffer CBObj : register(b1) { row_major float4x4 gWorld; float4 gColor; };

struct VSIn { 
    float4 pos : POSITION; 
    float2 uv : TEXCOORD0; 
    float3 nrm : NORMAL; 
};
struct VSOut { 
    float4 svpos : SV_POSITION; 
    float3 worldPos: TEXCOORD0; 
    float3 normal : TEXCOORD1; 
    float2 uv : TEXCOORD2; 
};

VSOut main(VSIn v) { 
    VSOut o; 
    float4 wp = mul(v.pos, gWorld); 
    o.worldPos = wp.xyz; 
    float3 wn = mul(float4(v.nrm, 0), gWorld).xyz; 
    o.normal = normalize(wn); 
    float4 vp = mul(wp, gView); 
    o.svpos = mul(vp, gProj); 
    
    // UVスクロール (Y方向に時間でスクロールさせる)
    // gColor.x = flowSpeed, gColor.y = uvScale
    o.uv = (v.uv * gColor.y) + float2(0, -gTime * gColor.x);
    
    // Y方向に微小な波打ち
    o.svpos.y -= sin(gTime * 2.0f + o.worldPos.x + o.worldPos.z) * 0.05f;

    return o; 
}
