#include "Obj.hlsli"
#include "Space.hlsli"
#include "ProceduralPaper.hlsli" // ★追加: 和紙マテリアル

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

// 距離減衰計算
float AttenDist(float3 atten, float d)
{
    return 1.0 / (atten.x + atten.y * d + atten.z * d * d);
}

float4 main(VSOutput input) : SV_TARGET
{
    // UV変換
    float2 uv = float2(
        input.uv.x * m_uv_scale.x + m_uv_offset.x,
        input.uv.y * m_uv_scale.y + m_uv_offset.y
    );
    float4 texcolor = tex.Sample(smp, uv);

    // 基本ベクトル
    float normalLen = length(input.normal);
    float3 N = normalLen > 0.0001 ? input.normal / normalLen : float3(0, 1, 0);
    
    float3 viewDir = cameraPos - input.worldpos.xyz;
    float viewDist = length(viewDir);
    float3 V = viewDist > 0.0001 ? viewDir / viewDist : float3(0, 0, 1);
    
    // ベースカラー
    float3 albedo = texcolor.rgb * color.rgb;

    // ★追加: プロシージャル和紙マテリアルを適用 (ベースカラーと法線を調整)
    ApplyProceduralPaper(input.worldpos.xyz, albedo, N, 0.4, 0.8);

    // アンビエント成分
    float3 finalColor = albedo * ambientColor;

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
            float spec = pow(saturate(dot(N, H)), 32.0f); // Shininess=32

            float3 diffuse = albedo * NdotL;
            float3 specular = m_specular * spec;

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
                float spec = pow(saturate(dot(N, H)), 32.0f);

                float3 diffuse = albedo * NdotL;
                float3 specular = m_specular * spec;

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
                float spec = pow(saturate(dot(N, H)), 32.0f);

                float3 diffuse = albedo * NdotL;
                float3 specular = m_specular * spec;

                finalColor += (diffuse + specular) * spotLights[k].color * att * ang;
            }
        }
    }

    // ★環境マッピング: 周囲の映り込み (Environment Mapping / Reflection)
    // 視線の反射ベクトルを計算
    float3 reflectDir = reflect(-V, N);
    
    // プロシージャル宇宙空間から環境色を取得 (軽量版でTDR回避)
    float3 envColor = GetReflectionEnvColor(reflectDir, time);
    
    // フレネル近似 (Schlick) で視線角度に応じた反射強度を計算
    // F0 = 基本反射率 (金属: 高い, 非金属: 低い)
    float F0 = 0.04; // 非金属のデフォルト反射率
    float fresnel = F0 + (1.0 - F0) * pow(max(1.0 - saturate(dot(N, V)), 0.0), 5.0);
    
    // 環境反射の合成
    // reflectivity: CBObjから取得した環境マップ反射率
    float reflectAmount = reflectivity * saturate(fresnel + reflectivity * 0.5);
    
    finalColor = lerp(finalColor, envColor, reflectAmount);

    return float4(finalColor, texcolor.a * color.a);
}