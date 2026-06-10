#pragma pack_matrix(row_major)

struct VSInput {
	float2 pos : POSITION;
	float2 uv : TEXCOORD;
	float4 color : COLOR;
};

struct VSOutput {
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
	float4 color : COLOR;
};
