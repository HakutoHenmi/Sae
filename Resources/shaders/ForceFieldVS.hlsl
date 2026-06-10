#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD) {
	float4 worldNormal = normalize(mul(float4(normal, 0), world));
	// シールドはメッシュよりわずかに大きくなるように膨らませる
	float4 expandedPos = pos + float4(normal * 0.1, 0.0);
	float4 worldPos = mul(expandedPos, world);

	VSOutput output;
	output.svpos = mul(expandedPos, mul(world, mul(view, projection)));
	output.worldpos = worldPos;
	output.normal = worldNormal.xyz;
	output.uv = uv;

	return output;
}
