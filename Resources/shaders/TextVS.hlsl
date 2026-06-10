#include "Text.hlsli"

VSOutput main(VSInput input) {
	VSOutput output;
	output.svpos = float4(input.pos, 0.0f, 1.0f);
	output.uv = input.uv;
	output.color = input.color;
	return output;
}
