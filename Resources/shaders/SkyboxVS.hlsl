#pragma pack_matrix(row_major)

cbuffer CBFrame : register(b0) {
    matrix gView;
    matrix gProj;
    matrix gViewProj;
    float3 gCameraPos;
    float  gTime;
};

struct VSIn {
    float3 pos : POSITION;
};

struct VSOut {
    float4 svpos : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

VSOut main(VSIn v) {
    VSOut o;
    // キューブマップのサンプリング方向 = 頂点のローカル座標そのもの
    o.texCoord = v.pos;

    // View行列から平行移動成分を除去（カメラ位置を無視して天球を常に中心に表示）
    matrix viewNoTranslate = gView;
    viewNoTranslate[3][0] = 0.0f;
    viewNoTranslate[3][1] = 0.0f;
    viewNoTranslate[3][2] = 0.0f;

    float4 clipPos = mul(float4(v.pos, 1.0f), mul(viewNoTranslate, gProj));
    // 深度を最遠に固定 (z = w → NDC z = 1.0)
    o.svpos = clipPos.xyww;

    return o;
}
