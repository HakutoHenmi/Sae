#include "Obj.hlsli"

Texture2D<float4> tex : register(t3);
TextureCube<float4> envMap : register(t7);
SamplerState smp : register(s0);

float4 main(VSOutput input) : SV_TARGET {
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);

    float3 R = reflect(-V, N);
    float4 envColor = float4(0, 0, 0, 0);
    if (useCubemap) {
        envColor = envMap.SampleLevel(smp, R, 1.0f);
    }

    float ndotv = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - ndotv, 2.5);
    float fresnelGlow = pow(1.0 - ndotv, 1.2);

    float3 diffuseLight = ambientColor * 0.6f;
    float3 specularLight = float3(0, 0, 0);

    if (dirLights[0].enabled) {
        float3 L = normalize(-dirLights[0].direction);
        float ndotl = max(dot(N, L), 0.0);
        diffuseLight += dirLights[0].color * ndotl * 0.55f;

        // ゼリー内部の透過光
        float sss = pow(saturate(dot(V, -L)), 2.5) * 0.65f;
        diffuseLight += color.rgb * sss;

        float3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 180.0);
        specularLight += dirLights[0].color * spec * 2.5f;
    }

    float3 slimeColor = color.rgb;
    float envReflectionStrength = reflectivity > 0 ? reflectivity : 1.2f;

    float3 finalColor = slimeColor * (0.35f + ndotv * 0.45f)
                      + envColor.rgb * envReflectionStrength * fresnel * 0.7f
                      + specularLight
                      + fresnelGlow * slimeColor * 1.4f
                      + fresnel * float3(0.55f, 1.0f, 0.65f);

    // 中心は透け、輪郭はしっかり見えるゼリー
    float alpha = lerp(0.35f, 0.9f, fresnelGlow) * color.a;
    return float4(finalColor, saturate(alpha));
}
