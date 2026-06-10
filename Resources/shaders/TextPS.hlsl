#include "Text.hlsli"

Texture2D<float> tex : register(t0); // R8_UNORM texture
SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET {
	float textAlpha = tex.Sample(smp, input.uv);
	float4 finalColor = input.color;
	finalColor.a *= textAlpha;
	return finalColor;
}
