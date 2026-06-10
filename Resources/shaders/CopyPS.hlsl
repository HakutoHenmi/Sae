Texture2D gScene : register(t0); 
SamplerState gSmp : register(s0);

float4 main(float4 svpos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET {
    return float4(gScene.Sample(gSmp, uv).rgb, 1.0f);
}
