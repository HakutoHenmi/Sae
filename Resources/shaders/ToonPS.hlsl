#include "Obj.hlsli"
#include "ProceduralPaper.hlsli" // ★追加: 和紙マテリアル

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

// 距離減衰計算
float AttenDist(float3 atten, float d)
{
    return 1.0 / (atten.x + atten.y * d + atten.z * d * d);
}

// ★ アニメ調(セルルック)のライティング計算
float3 CalculateAnimeLighting(float3 baseColor, float NdotL, float3 lightColor, float3 shadowTint)
{
    float3 shadowColor = baseColor * shadowTint;
    float halfLambert = NdotL * 0.5f + 0.5f;
    float toonFactor = smoothstep(0.48f, 0.52f, halfLambert);
    float3 diffuse = lerp(shadowColor, baseColor, toonFactor);
    return diffuse * lightColor;
}


float4 main(VSOutput input) : SV_TARGET
{
    // 1. テクスチャとオブジェクト色の取得
    float4 texColor = tex.Sample(smp, input.uv);
    float3 baseColor = texColor.rgb * color.rgb;

    // アルファテスト
    if (texColor.a * color.a < 0.1f) {
        discard;
    }

    // 2. 基本ベクトル
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);

    // ★追加: プロシージャル和紙マテリアルを適用
    ApplyProceduralPaper(input.worldpos.xyz, baseColor, N, 0.4, 0.8);

    // 3. 影の色味
    float3 shadowTint = float3(0.5f, 0.4f, 0.6f); 
    float3 ambientShadow = ambientColor * shadowTint * 0.8f;
    float3 finalColor = baseColor * ambientColor * 0.3f;

    // Directional Lights
    for (int i = 0; i < MAX_DIR_LIGHTS; ++i)
    {
        if (dirLights[i].enabled != 0)
        {
            float3 L = normalize(-dirLights[i].direction);
            float NdotL = dot(N, L);
            finalColor += CalculateAnimeLighting(baseColor, NdotL, dirLights[i].color, ambientShadow);
        }
    }

    // Point Lights
    for (int j = 0; j < MAX_POINT_LIGHTS; ++j)
    {
        if (pointLights[j].enabled != 0)
        {
            float3 Lvec = pointLights[j].position - input.worldpos.xyz;
            float d = length(Lvec);
            if (d < pointLights[j].range)
            {
                float3 L = Lvec / max(d, 1e-5);
                float att = AttenDist(pointLights[j].atten, d);
                float NdotL = dot(N, L);
                finalColor += CalculateAnimeLighting(baseColor, NdotL, pointLights[j].color * att, ambientShadow);
            }
        }
    }

    // Spot Lights
    for (int k = 0; k < MAX_SPOT_LIGHTS; ++k)
    {
        if (spotLights[k].enabled != 0)
        {
            float3 Lvec = spotLights[k].position - input.worldpos.xyz;
            float d = length(Lvec);
            if (d < spotLights[k].range)
            {
                float3 L = Lvec / max(d, 1e-5);
                float cosAng = dot(L, normalize(spotLights[k].direction));
                float ang = smoothstep(spotLights[k].outerCos, spotLights[k].innerCos, cosAng);
                float att = AttenDist(spotLights[k].atten, d);
                float NdotL = dot(N, L);
                finalColor += CalculateAnimeLighting(baseColor, NdotL, spotLights[k].color * att * ang, ambientShadow);
            }
        }
    }

    // リムライト
    float NdotV = saturate(dot(N, V));
    float rim = 1.0f - NdotV;
    float rimPow = pow(rim, 4.0f);
    float rimIntensity = smoothstep(0.6f, 0.7f, rimPow);
    float3 rimColor = baseColor * 2.0f;
    finalColor += rimColor * rimIntensity * 0.4f;

    return float4(finalColor, texColor.a * color.a);
}