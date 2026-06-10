// ------------- Resources/Shaders/Water.hlsl -------------

// ===== テクスチャ & サンプラ =====
Texture2D gNormalTex0 : register(t0); // 法線マップ1
Texture2D gNormalTex1 : register(t1); // 法線マップ2
SamplerState gSampler : register(s0);

// ===== カメラ共通 CB（普段使っているものに合わせて） =====
cbuffer CameraCB : register(b0)
{
    float4x4 gViewProj; // 使わなくても OK（他と合わせるため）
    float3 gCameraPos;
    float _pad0;
};

// ===== 水専用 CB =====
cbuffer WaterCB : register(b1)
{
    float gTime; // 経過時間
    float gWaveSpeed1; // 法線マップ1のスクロール速度
    float gWaveSpeed2; // 法線マップ2のスクロール速度
    float gFresnelPower; // フレネルの鋭さ（4〜6くらい）

    float gFresnelScale; // フレネルの強さ
    float3 gShallowColor; // 浅い部分の色 (RGB)
    float3 gDeepColor; // 深い部分の色 (RGB)
    float gWaterHeight; // 水面の Y 高さ（0 なら原点）
};

// ===== 頂点シェーダ入力 / 出力 =====
struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD0;
};

// ===== 頂点シェーダ =====
// ここでは「すでにワールド変換済みの頂点」を渡す前提にしています。
// （あなたのエンジンで World * ViewProj をやるならそこに合わせてください）
VSOutput VSMain(VSInput vin)
{
    VSOutput vout;
    vout.worldPos = vin.position;
    vout.position = mul(float4(vin.position, 1.0f), gViewProj); // 必要なら
    vout.uv = vin.uv;
    return vout;
}

// ===== フレネル計算のヘルパー =====
float FresnelTerm(float3 viewDir, float3 normal, float power, float scale)
{
    float cosTheta = saturate(dot(viewDir, normal));
    float f = pow(1.0f - cosTheta, power) * scale;
    return saturate(f);
}

// ===== ピクセルシェーダ =====
float4 PSMain(VSOutput pin) : SV_TARGET
{
    // ---- 法線マップスクロール ----
    float2 uv1 = pin.uv + float2(gTime * gWaveSpeed1, 0.0f);
    float2 uv2 = pin.uv + float2(0.0f, gTime * gWaveSpeed2);

    float3 n1 = gNormalTex0.Sample(gSampler, uv1).xyz * 2.0f - 1.0f;
    float3 n2 = gNormalTex1.Sample(gSampler, uv2).xyz * 2.0f - 1.0f;
    float3 normalWS = normalize(n1 + n2); // 合成

    // ---- ビュー方向 ----
    float3 viewDir = normalize(gCameraPos - pin.worldPos);

    // ---- フレネルによる反射強度 ----
    float fresnel = FresnelTerm(viewDir, normalWS, gFresnelPower, gFresnelScale);

    // ---- 簡易ライティング：上からの光を想定した N・L ----
    float3 lightDir = normalize(float3(-0.3f, -1.0f, -0.2f)); // 適当な斜め上から
    float ndotl = saturate(dot(normalWS, -lightDir));
    
    // ---- 深さによる色（鳴潮っぽく浅瀬が明るい）----
    float heightDiff = gWaterHeight - pin.worldPos.y; // プラス: 深くなる
    float depth01 = saturate(heightDiff * 0.1f); // スケールは好みで
    float3 baseColor = lerp(gShallowColor, gDeepColor, depth01);

    // ---- 反射色（空色を仮に浅い青で）----
    float3 skyColor = float3(0.35f, 0.55f, 0.95f);
    float3 reflectionColor = skyColor;

    // ---- 拡散＋反射ブレンド ----
    float3 diffuse = baseColor * (0.4f + 0.6f * ndotl); // 0.4〜1.0 の範囲
    float3 color = lerp(diffuse, reflectionColor, fresnel);

    // 透明度：浅瀬は透けて見えるように alpha を少し変える
    float alpha = lerp(0.6f, 0.9f, depth01); // 深いほど濃い

    return float4(color, alpha);
}
