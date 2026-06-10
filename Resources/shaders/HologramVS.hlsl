#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD) {
	// ホログラム特有の頂点揺れ (グリッチ効果)
	float glitchOffset = sin(pos.y * 50.0 + time * 10.0) * 0.05;
	float4 perturbedPos = pos + float4(glitchOffset * normal, 0.0);

	float4 worldNormal = normalize(mul(float4(normal, 0), world));
	float4 worldPos = mul(perturbedPos, world);

	VSOutput output;
	output.svpos = mul(perturbedPos, mul(world, mul(view, projection)));
	output.worldpos = worldPos;
	output.normal = worldNormal.xyz;
	output.uv = uv;

	return output;
}
