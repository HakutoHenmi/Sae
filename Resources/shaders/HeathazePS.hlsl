#include "Obj.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

// 距離減衰計算
float AttenDist(float3 atten, float d)
{
    return 1.0 / (atten.x + atten.y * d + atten.z * d * d);
}

float4 main(VSOutput input) : SV_TARGET
{
    // 時間でUVを揺らす（ヒートヘイズ処理）
    float2 distUV = input.uv;
    float strength = 0.02f; // 歪みの強さ
    float speed = 5.0f; // 揺れの速さ
    
    // sin波でUVをずらす
    distUV.x += sin(distUV.y * 20.0f + time * speed) * strength;
    distUV.y += cos(distUV.x * 20.0f + time * speed) * strength;

    // テクスチャサンプリング
    float4 texcolor = tex.Sample(smp, distUV);

    // ライティング計算
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldpos.xyz);
    float3 albedo = texcolor.rgb * color.rgb;
    float3 finalColor = albedo * ambientColor; // 環境光

    // ----------------------------------------------------
    // Directional Lights
    // ----------------------------------------------------
    for (int i = 0; i < MAX_DIR_LIGHTS; ++i)
    {
        if (dirLights[i].enabled != 0)
        {
            float3 L = normalize(-dirLights[i].direction);
            float NdotL = saturate(dot(N, L));
            
            float3 H = normalize(L + V);
            float spec = pow(saturate(dot(N, H)), 10.0f); // 簡易スペキュラ
            
            float3 diffuse = albedo * NdotL;
            float3 specular = float3(spec, spec, spec) * 0.5f;

            finalColor += (diffuse + specular) * dirLights[i].color;
        }
    }

    // ----------------------------------------------------
    // Point Lights
    // ----------------------------------------------------
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

                float NdotL = saturate(dot(N, L));
                float3 H = normalize(L + V);
                float spec = pow(saturate(dot(N, H)), 10.0f);

                float3 diffuse = albedo * NdotL;
                float3 specular = float3(spec, spec, spec) * 0.5f;

                finalColor += (diffuse + specular) * pointLights[j].color * att;
            }
        }
    }

    // ----------------------------------------------------
    // Spot Lights
    // ----------------------------------------------------
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

                float NdotL = saturate(dot(N, L));
                float3 H = normalize(L + V);
                float spec = pow(saturate(dot(N, H)), 10.0f);

                float3 diffuse = albedo * NdotL;
                float3 specular = float3(spec, spec, spec) * 0.5f;

                finalColor += (diffuse + specular) * spotLights[k].color * att * ang;
            }
        }
    }

    return float4(finalColor, texcolor.a * color.a);
}