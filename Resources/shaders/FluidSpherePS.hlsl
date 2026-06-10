// FluidSpherePS.hlsl
#pragma pack_matrix(row_major)

cbuffer ViewProjection : register(b0) {
    matrix view;
    matrix projection;
    matrix viewProj;
    matrix invProjection;
    float3 cameraPos;
    float time;
    float3 corePosition;
    float isLiquidated;
};

struct VSOutput {
    float4 pos : SV_POSITION; // Screen pos
    float2 uv : TEXCOORD;     // -1 to 1
    float3 viewPos : POSITION0;
    float3 worldPos : POSITION1;
    float3 centerWorldPos : POSITION2;
    float radius : BLENDWEIGHT0;
    float3 color : COLOR;
};

struct PSOutput {
    float4 colorOut : SV_TARGET0;
    float depth : SV_Depth;
};

PSOutput main(VSOutput input) {
    PSOutput output;
    
    // UVからの距離（中心0、エッジ1）
    float distFromCenter = length(input.uv);
    if (distFromCenter > 1.0f) discard;
    
    float z = sqrt(1.0f - dot(input.uv, input.uv));
    
    // ピクセルのビュー空間座標を計算
    float3 pixelViewPos = input.viewPos;
    pixelViewPos.z -= z * input.radius; 
    
    // クリップ空間へ変換して Z バッファ用の深度を計算
    float4 clipPos = mul(float4(pixelViewPos, 1.0f), projection);
    output.depth = clipPos.z / clipPos.w;
    
    // 球体の簡易ライティング
    float3 normal = normalize(float3(input.uv.x, input.uv.y, -z));
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f));
    float diff = max(dot(normal, lightDir), 0.3f);
    
    // リムライト・フレネル（縁の光沢）
    float rim = 1.0f - max(dot(normal, float3(0, 0, -1)), 0.0f);
    float fresnel = pow(rim, 3.0f);
    
    // スペキュラ（ハイライト）
    float3 viewDir = float3(0, 0, -1);
    float3 halfVec = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfVec), 0.0f), 200.0f);
    
    // エッジのソフトフェード
    float edgeFade = smoothstep(1.0f, 0.3f, distFromCenter);
    
    // 参考画像風: 小さい粒がドームの中で見える + 透明な外殻
    float3 baseColor = input.color * diff;
    float3 finalColor = baseColor * 0.8f + float3(0.5f, 0.8f, 0.9f) * fresnel * 0.4f + float3(1, 1, 1) * spec * 2.0f;
    
    // 中心は不透明で色がしっかり見え、エッジは透ける
    float alpha = edgeFade * 0.35f + fresnel * 0.3f + spec * 0.8f;
    
    output.colorOut = float4(finalColor, alpha);
    
    return output;
}
