struct VSOut {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

float4 main(VSOut i) : SV_TARGET {
    // UVを-1.0〜1.0に変換し、中心からの距離で円形にマスクする
    float2 uv = i.uv * 2.0 - 1.0;
    float dist = dot(uv, uv);
    if(dist > 1.0) discard;
    
    // 中心ほど濃く、縁へ行くほど薄くなるようにアルファを計算
    float alpha = 1.0 - dist;
    float4 c = i.color;
    c.a *= alpha;
    
    if(c.a < 0.01) discard;
    return c;
}
