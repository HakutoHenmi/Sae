// Engine/Water/WaterSurface.cpp
#include "WaterSurface.h"
#include "Camera.h"
#include "WindowDX.h"

#include <cassert>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

#ifndef HR_CHECK
#define HR_CHECK(x)                                                                                                                                                                                    \
	do {                                                                                                                                                                                               \
		HRESULT __hr__ = (x);                                                                                                                                                                          \
		if (FAILED(__hr__)) {                                                                                                                                                                          \
			char buf[256];                                                                                                                                                                             \
			sprintf_s(buf, "HR failed 0x%08X at %s(%d)\n", (unsigned)__hr__, __FILE__, __LINE__);                                                                                                      \
			OutputDebugStringA(buf);                                                                                                                                                                   \
			assert(false && "D3D12 call failed");                                                                                                                                                      \
			std::abort();                                                                                                                                                                              \
		}                                                                                                                                                                                              \
	} while (0)
#endif

namespace {

// シェーダコンパイルヘルパー（Renderer::Compile と同じイメージ）
ComPtr<ID3DBlob> CompileShader(const char* src, const char* entry, const char* target) {
	UINT fl = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	fl |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ComPtr<ID3DBlob> s, e;
	HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, fl, 0, &s, &e);
	if (FAILED(hr)) {
		if (e) {
			MessageBoxA(nullptr, (const char*)e->GetBufferPointer(), "Water HLSL Compile Error", MB_OK);
		}
		std::abort();
	}
	return s;
}

// ---------------- HLSL：水面 ----------------
//
// 第一段階：
//  - 頂点シェーダで 2 つの波による高さを計算
//  - 法線を数値微分で求める
//  - ピクセルシェーダで簡単なフレネル＋ライティング
//
static const char* gVSWater = R"(

cbuffer CBCommon : register(b0)
{
    float4x4 g_mvp;
    float4   g_color;
    float4   g_camPos; // PS とレイアウトをそろえる用
};

cbuffer CBWave : register(b1)
{
    float4 g_wave1; // dirX, dirZ, amplitude, frequency
    float4 g_wave2; // dirX, dirZ, amplitude, frequency
    float4 g_misc;  // time, waterHeight, _, _
};

struct VSIn {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VSOut {
    float4 sp       : SV_Position;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

// ===== 多周波の高さ関数 =====
float heightAt(float2 xz)
{
    float t = g_misc.x;
    float h = g_misc.y; // ベース高さ

    // --- 1) 大きいうねり（ここは今のままでもOK） ---
    float2 d1 = normalize(g_wave1.xy);
    float  a1 = g_wave1.z;
    float  f1 = g_wave1.w;

    float2 d2 = normalize(g_wave2.xy);
    float  a2 = g_wave2.z;
    float  f2 = g_wave2.w;

    h += sin(dot(d1, xz) * f1 + t * f1) * a1;
    h += sin(dot(d2, xz) * f2 + t * f2 * 1.1) * a2;

    // --- 2) 中くらいの波（かなり強く＆細かく） ---
    {
        float2 d3 = normalize(float2( 0.8,  0.6));
        float2 d4 = normalize(float2(-0.6,  0.9));

        // ★ここを強める
        float  aMid = 0.18;   // 振幅 (前: 0.08)
        float  kMid = 0.35;   // 波数 (前: 0.20) → 細かく
        float  sMid = 0.9;    // 速さ (前: 0.7)  → 少し速く

        h += sin(dot(d3, xz) * kMid        + t * sMid      ) * aMid;
        h += sin(dot(d4, xz) * kMid * 1.3  + t * (sMid*1.2)) * aMid;
    }

    // --- 3) 細かいさざ波（全体にびっしり） ---
    {
        // ★振幅と細かさをアップ
        float  aSmall = 0.06; // (前: 0.03)
        float  kS1 = 0.90;    // (前: 0.55)
        float  kS2 = 1.30;    // (前: 0.75)

        float2 r1 = xz * kS1 + float2(t * 1.3,  t * 0.9);
        float2 r2 = xz * kS2 + float2(-t * 1.0, t * 1.2);

        float sr1 = sin(r1.x) * cos(r1.y * 1.3);
        float sr2 = cos(r2.x * 1.2) * sin(r2.y);

        h += (sr1 + sr2) * 0.5 * aSmall;
    }

    // --- 4) 極小の速い波（シルエット用） ---
    {
        // ★ここも少し上げる
        float aTiny = 0.04;   // (前: 0.02)
        float kT1 = 1.9;      // (前: 1.1)
        float kT2 = 2.5;      // (前: 1.6)

        float2 s1 = xz * kT1 + float2(t * 1.4,  t * 1.0);
        float2 s2 = xz * kT2 + float2(-t * 1.1, t * 1.6);

        float tw1 = sin(s1.x) * cos(s1.y * 1.1);
        float tw2 = cos(s2.x * 1.3) * sin(s2.y);

        h += (tw1 + tw2) * 0.5 * aTiny;
    }

    return h;
}

VSOut main(VSIn i)
{
    VSOut o;

    float3 p = i.pos;

    // 高さを計算（多周波）
    p.y = heightAt(p.xz);

    // 数値微分で法線（高さ関数と整合するように）
    float eps = 0.4;
    float h_dx = heightAt(p.xz + float2(eps, 0.0)) - heightAt(p.xz - float2(eps, 0.0));
    float h_dz = heightAt(p.xz + float2(0.0, eps)) - heightAt(p.xz - float2(0.0, eps));
    float3 n = normalize(float3(-h_dx, 2.0 * eps, -h_dz));

    o.worldPos = p;
    o.normal   = n;
    o.uv       = i.uv;

    o.sp = mul(float4(p, 1.0f), g_mvp);
    return o;
}

)";

static const char* gPSWater = R"(

cbuffer CBCommon : register(b0)
{
    float4x4 g_mvp;
    float4   g_color;
    float4   g_camPos; // xyz: camera pos
};

cbuffer CBWave : register(b1)
{
    float4 g_wave1; // dirX, dirZ, amplitude, frequency
    float4 g_wave2; // dirX, dirZ, amplitude, frequency
    float4 g_misc;  // time, waterHeight, _, _
};

struct PSIn {
    float4 sp       : SV_Position;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

float4 main(PSIn i) : SV_Target
{
    float time = g_misc.x;

    // ===== 基本ベクトル =====
    float3 N = normalize(i.normal);
    float3 L = normalize(float3(-0.3, 1.0, -0.4));        // 太陽方向
    float3 V = normalize(g_camPos.xyz - i.worldPos);      // ビュー
    float3 H = normalize(L + V);

    float ndl = saturate(dot(N, L));
    float ndh = saturate(dot(N, H));

    // ===== フレネル =====
    float fresnel = pow(1.0 - saturate(dot(N, V)), 5.0);

    // ===== ベースの水色（少し落ち着いた色に）=====
    float3 colDeep    = float3(0.02, 0.12, 0.28); // 深い青
    float3 colShallow = float3(0.08, 0.50, 0.78); // 浅いシアン

    float crest = saturate((i.worldPos.y - (g_misc.y - (g_wave1.z + g_wave2.z)))
                           / max(g_wave1.z + g_wave2.z, 0.01));
    float3 waterBase = lerp(colDeep, colShallow, crest);

    // ===== 空の色（Skybox 風グラデ）=====
    float tSky = saturate(N.y * 0.5 + 0.5);
    float3 skyBottom = float3(0.16, 0.36, 0.60);
    float3 skyTop    = float3(0.65, 0.90, 1.00);
    float3 skyCol    = lerp(skyBottom, skyTop, tSky);

    // ===== 小さなさざ波（少し弱め）=====
    float2 smallUV1 = i.worldPos.xz * 1.6 + float2(time * 0.7, time * 0.25);
    float2 smallUV2 = i.worldPos.xz * 2.2 + float2(-time * 0.45, time * 0.55);

    float s1 = sin(smallUV1.x) * cos(smallUV1.y * 1.3);
    float s2 = cos(smallUV2.x * 1.6) * sin(smallUV2.y);

    float smallWave = (s1 + s2) * 0.5; // -1～1

    // 法線にちょっとだけ影響させる（前より弱く）
    float3 nPerturb = float3(s1 * 0.10, 0.0, s2 * 0.10);
    N = normalize(N + nPerturb);

    // N を変えたので再計算
    ndl = saturate(dot(N, L));
    H   = normalize(L + V);
    ndh = saturate(dot(N, H));

    // ===== コースティクス（細かく & 弱め）=====
    float2 cUv1 = i.worldPos.xz * 0.35 + float2(time * 0.30, 0.0);
    float2 cUv2 = i.worldPos.xz * 0.42 + float2(0.0, time * 0.36);

    float c1 = sin(cUv1.x) * sin(cUv1.y * 1.4);
    float c2 = sin(cUv2.x * 1.3 + cUv2.y) * sin(cUv2.y * 1.8);

    float caustic = (c1 + c2) * 0.5;     // -1～1
    caustic = caustic * 0.5 + 0.5;       // 0～1
    caustic = pow(caustic, 3.5);         // 線だけ残るように

    // フレネルで水色と空色をブレンド
    float3 mixCol = lerp(waterBase, skyCol, fresnel);

    // コースティクスの強さ（前よりかなり控えめ）
    float caMul = lerp(0.9, 1.25, caustic);
    mixCol *= caMul;

    // ===== ディフューズ + スペキュラ =====
    float3 diffuse  = mixCol * (0.30 + 0.70 * ndl);

    float specPower = 70.0;  // ハイライトの鋭さ
    float specStr   = 0.9;   // 強さ（前より弱め）
    float spec = pow(ndh, specPower) * specStr;

    float3 finalCol = diffuse + spec;

   // ===== 波の山に白い泡を乗せる =====

    // 水面ベース高さとの差分（山ほど + 側）
    float crestHeight = saturate(
        (i.worldPos.y - g_misc.y) / max(g_wave1.z + g_wave2.z, 0.01)
    );

    // 面の傾き（フラットだと 0、立ってるほど 1）
    float slope = saturate(1.0 - N.y);

    // 泡マスク（山 ＋ 傾き） → 少しシビアにしぼる
    float foamMask = crestHeight * 1.8 + slope * 0.6 - 0.4;
    foamMask = saturate(foamMask);
    foamMask = pow(foamMask, 2.5); // エッジだけ残るように

    // 泡の色（ほぼ白だけど少し青寄り）
    float3 foamCol = float3(0.95, 0.98, 1.0);

    // 強さ（好みで 0.3～1.0 の間で）
    float foamStrength = 0.7;

    // 色ブレンド：泡マスクが高い場所ほど白く
    finalCol = lerp(finalCol, foamCol, foamMask * foamStrength);

    // 泡のところはちょっと不透明に
    float alphaBase = lerp(0.55, 0.90, fresnel);
    float alpha = saturate(alphaBase + foamMask * 0.15);

    return float4(finalCol, alpha);
}

)";

} // anonymous namespace

namespace Engine {

bool WaterSurface::Initialize(WindowDX& dx, const WaterSurfaceDesc& desc) {
	dx_ = &dx;
	desc_ = desc;

	// 波パラメータ初期化
	waveParam_.wave1 = DirectX::XMFLOAT4(1.0f, 0.3f, 0.45f, 0.08f);  // amp を 0.45 に
	waveParam_.wave2 = DirectX::XMFLOAT4(-0.4f, 1.0f, 0.30f, 0.15f); // amp を 0.30 に
	waveParam_.misc = DirectX::XMFLOAT4(0.0f, desc_.height, 0.0f, 0.0f);

	if (!createMesh_(dx))
		return false;
	if (!createPipeline_(dx))
		return false;

	return true;
}

void WaterSurface::Shutdown() {
	vb_.Reset();
	ib_.Reset();
	cbCommon_.Reset();
	cbWave_.Reset();
	rs_.Reset();
	pso_.Reset();
	indexCount_ = 0;
	dx_ = nullptr;
}

// dt は 1/60 などで呼び出し
void WaterSurface::Update(float deltaSeconds) {
	time_ += deltaSeconds;
	waveParam_.misc.x = time_;
}

void WaterSurface::Draw(ID3D12GraphicsCommandList* cmd, const Camera& cam) {
	if (!cmd || !pso_ || !vb_ || !ib_) {
		return;
	}

	// ---- b0: WVP + 色 + カメラ位置 ----
	CBCommon cb{};
	cb.color = DirectX::XMFLOAT4(1, 1, 1, 1);

	using namespace DirectX;
	XMMATRIX W = XMMatrixIdentity();
	XMMATRIX V = cam.View();
	XMMATRIX P = cam.Proj();
	XMMATRIX MVP = W * V * P;
	XMStoreFloat4x4(&cb.mvp, XMMatrixTranspose(MVP));

	// ★ カメラ位置をセット
	auto camPos = cam.Position();
	cb.camPos = XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);

	void* p = nullptr;
	cbCommon_->Map(0, nullptr, &p);
	memcpy(p, &cb, sizeof(cb));
	cbCommon_->Unmap(0, nullptr);

	// ---- b1: 波パラメータ ----
	cbWave_->Map(0, nullptr, &p);
	memcpy(p, &waveParam_, sizeof(waveParam_));
	cbWave_->Unmap(0, nullptr);

	cmd->SetPipelineState(pso_.Get());
	cmd->SetGraphicsRootSignature(rs_.Get());

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 1, &vbv_);
	cmd->IASetIndexBuffer(&ibv_);

	cmd->SetGraphicsRootConstantBufferView(0, cbCommon_->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(1, cbWave_->GetGPUVirtualAddress());

	cmd->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

// ------------ 内部：メッシュ生成 ------------
bool WaterSurface::createMesh_(WindowDX& dx) {
	const unsigned int vxCountX = desc_.tessX + 1;
	const unsigned int vxCountZ = desc_.tessZ + 1;

	const float halfX = desc_.sizeX * 0.5f;
	const float halfZ = desc_.sizeZ * 0.5f;

	std::vector<Vertex> verts;
	verts.resize(static_cast<size_t>(vxCountX) * vxCountZ);

	// X,Z 平面を原点中心で作成（Y=0。高さはシェーダで）
	for (unsigned int z = 0; z < vxCountZ; ++z) {
		float tz = static_cast<float>(z) / desc_.tessZ;
		float posZ = -halfZ + desc_.sizeZ * tz;

		for (unsigned int x = 0; x < vxCountX; ++x) {
			float tx = static_cast<float>(x) / desc_.tessX;
			float posX = -halfX + desc_.sizeX * tx;

			Vertex v{};
			v.pos = XMFLOAT3(posX, 0.0f, posZ);
			v.uv = XMFLOAT2(tx, tz);

			verts[z * vxCountX + x] = v;
		}
	}

	// インデックス
	std::vector<uint32_t> indices;
	indices.reserve(static_cast<size_t>(desc_.tessX) * desc_.tessZ * 6);

	for (unsigned int z = 0; z < desc_.tessZ; ++z) {
		for (unsigned int x = 0; x < desc_.tessX; ++x) {
			uint32_t i0 = z * vxCountX + x;
			uint32_t i1 = z * vxCountX + (x + 1);
			uint32_t i2 = (z + 1) * vxCountX + x;
			uint32_t i3 = (z + 1) * vxCountX + (x + 1);

			// 頂点順を i0, i2, i1 / i1, i2, i3 に変更
			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);

			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
		}
	}

	indexCount_ = static_cast<unsigned int>(indices.size());

	// Upload ヒープで作成（静的なのでこれでOK）
	CD3DX12_HEAP_PROPERTIES hpU(D3D12_HEAP_TYPE_UPLOAD);

	// VB
	{
		UINT64 sizeVB = sizeof(Vertex) * verts.size();
		auto rd = CD3DX12_RESOURCE_DESC::Buffer(sizeVB);
		HR_CHECK(dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vb_)));

		void* p = nullptr;
		vb_->Map(0, nullptr, &p);
		memcpy(p, verts.data(), sizeVB);
		vb_->Unmap(0, nullptr);

		vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
		vbv_.SizeInBytes = static_cast<UINT>(sizeVB);
		vbv_.StrideInBytes = sizeof(Vertex);
	}

	// IB
	{
		UINT64 sizeIB = sizeof(uint32_t) * indices.size();
		auto rd = CD3DX12_RESOURCE_DESC::Buffer(sizeIB);
		HR_CHECK(dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ib_)));

		void* p = nullptr;
		ib_->Map(0, nullptr, &p);
		memcpy(p, indices.data(), sizeIB);
		ib_->Unmap(0, nullptr);

		ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
		ibv_.SizeInBytes = static_cast<UINT>(sizeIB);
		ibv_.Format = DXGI_FORMAT_R32_UINT;
	}

	// CB (b0, b1)
	{
		auto rd = CD3DX12_RESOURCE_DESC::Buffer(256);
		HR_CHECK(dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cbCommon_)));

		auto rd2 = CD3DX12_RESOURCE_DESC::Buffer(256);
		HR_CHECK(dx.Dev()->CreateCommittedResource(&hpU, D3D12_HEAP_FLAG_NONE, &rd2, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cbWave_)));
	}

	return true;
}

// ------------ 内部：PSO 生成 ------------
bool WaterSurface::createPipeline_(WindowDX& dx) {
	// RootSignature: b0, b1 (CBV 2つだけ)
	CD3DX12_ROOT_PARAMETER rp[2];
	rp[0].InitAsConstantBufferView(0); // CBCommon
	rp[1].InitAsConstantBufferView(1); // CBWave

	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(_countof(rp), rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> sig, err;
	HR_CHECK(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err));
	HR_CHECK(dx.Dev()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rs_)));

	// シェーダ
	auto vs = CompileShader(gVSWater, "main", "vs_5_0");
	auto ps = CompileShader(gPSWater, "main", "ps_5_0");

	// 入力レイアウト
	D3D12_INPUT_ELEMENT_DESC il[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = rs_.Get();
	d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	d.InputLayout = {il, _countof(il)};

	d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	d.BlendState.RenderTarget[0].BlendEnable = TRUE;
	d.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	d.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	d.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// とりあえず不透明。半透明にしたいならここで BlendEnable を TRUE に
	d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	d.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	d.SampleMask = UINT_MAX;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 1;
	d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	d.SampleDesc.Count = 1;

	HR_CHECK(dx.Dev()->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso_)));

	return true;
}

} // namespace Engine
