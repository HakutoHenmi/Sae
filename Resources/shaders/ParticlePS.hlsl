Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
float4 main(float4 svpos:SV_POSITION, float2 uv:TEXCOORD, float4 color:COLOR) : SV_TARGET {
    float4 texColor = tex.Sample(smp, uv);
    texColor *= color;
    if (texColor.a <= 0.0f) { discard; }
    return texColor;
}
