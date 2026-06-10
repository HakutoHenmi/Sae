// 画面全体に貼るための簡易VS（UVだけ渡す）
// 既に持っているスプライト用VSがあるならそれを使ってOK

struct VSIn {
    float2 pos : POSITION;  // -1..+1 のクリップ空間頂点（全画面）
    float2 uv  : TEXCOORD0; // 0..1
};
struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
VSOut mainVS(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos, 0.0, 1.0);
    o.uv  = i.uv;
    return o;
}
