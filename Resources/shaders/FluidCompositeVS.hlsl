// FluidCompositeVS.hlsl
struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

// 頂点バッファなしでフルスクリーンQuadを描画するための魔法
VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput output;
    // 0, 1, 2の頂点IDから巨大な三角形を1つ作る
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.pos = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}
