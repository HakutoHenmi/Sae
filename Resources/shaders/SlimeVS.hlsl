#include "Obj.hlsli"

VSOutput main(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD) {
    float4 newPos = pos;
    float h = newPos.y;

    // おまんじゅう型スライム（参考: アニメのゼリー）
    // 底面を大きく平らにして接地感を出す
    if (h < 0.0f) {
        float bottom = smoothstep(0.0f, -1.0f, h);
        newPos.y = lerp(h, -0.92f, bottom * 0.9f);
        float spread = bottom * 0.42f;
        newPos.x += normal.x * spread;
        newPos.z += normal.z * spread;
    }

    // 上面は丸みを強調
    if (h > 0.0f) {
        newPos.y += pow(saturate(h), 1.4f) * 0.18f;
    }

    // ゆるいゼリーの揺れ（細かいノイズは使わない）
    float wobbleMask = smoothstep(-0.92f, 0.35f, newPos.y);
    float wave = sin(time * 2.2f + h * 2.8f) * 0.035f;
    float waveX = cos(time * 1.9f + h * 2.0f) * 0.028f;
    float waveZ = sin(time * 1.7f + h * 2.1f) * 0.028f;
    newPos.x += waveX * wobbleMask;
    newPos.y += wave * wobbleMask;
    newPos.z += waveZ * wobbleMask;

    // 底面法線を上向きに補正して平らな接地を見せる
    float3 smoothNormal = normal;
    if (newPos.y < -0.15f) {
        smoothNormal = normalize(lerp(smoothNormal, float3(0, 1, 0), smoothstep(-0.15f, -0.9f, newPos.y)));
    }

    float4 worldNormal = normalize(mul(float4(smoothNormal, 0), world));
    float4 worldPos = mul(newPos, world);

    VSOutput output;
    output.svpos = mul(newPos, mul(world, mul(view, projection)));
    output.worldpos = worldPos;
    output.normal = worldNormal.xyz;
    output.uv = uv;
    return output;
}
